# Prepare a PCD Map and Author a FAR Planner Mission

This guide converts an existing point-cloud map such as `dorsett.pcd` into a
consistent local `map` frame, derives FAR Planner boundary and visibility-graph
files, and opens RViz so an operator can record an initial pose and ordered
waypoints.

The current script names in this repository are:

- `src/spot_navigation/scripts/align_pointcloud_to_map_frame.py`
- `src/spot_navigation/scripts/build_far_prior_map.py`
- `src/spot_navigation/launch/mission_recorder.launch.py`

The first two are standalone Python scripts. They are not `ros2 run`
executables. The mission recorder is a ROS 2 launch workflow.

## Outputs and data flow

```text
<map>.pcd
   |
   | align_pointcloud_to_map_frame.py
   | operator clicks origin and +X direction
   v
<map>_transformed.pcd
   |
   | build_far_prior_map.py
   v
<map>_transformed_boundary.ply   boundary polygons and RViz overlay
<map>_transformed_trajectory.txt known-free point for FAR boundary topology
<map>_transformed.vgh            FAR prior visibility graph
<map>_transformed_boundary_stats.json
<map>_transformed_boundary_preview.png
   |
   | mission_recorder.launch.py
   | operator clicks one initial pose and ordered goal poses
   v
<map>_mission.yaml
<map>_mission.png
```

The `*.vgh` file, not the PCD or PLY file, is the prior graph loaded by FAR
Planner. The boundary PLY is the polygon source and is also displayed by the
mission-authoring RViz session. `build_far_prior_map.py` writes a FAR-compatible
VGH directly, so a separate `boundary_handler` conversion is not required.

## 1. Prepare the environment

Run commands from the workspace root. Replace `/path/to/quadrupled_ws` with the
location of your checkout. The transform tool needs Open3D, NumPy, Matplotlib,
and Tk. The FAR prior-map builder needs `pypcd4`, NumPy, and OpenCV. These
dependencies are included in the project Docker image. Build the image and
verify the imports from the host:

```bash
cd /path/to/quadrupled_ws
docker compose build ros-humble-dev
docker compose run --rm ros-humble-dev \
  python3 -c "import open3d, numpy, matplotlib, tkinter, cv2, pypcd4; print('map tools ready')"
```

For non-GUI work, create or recreate the persistent development container from
that image, then enter it:

```bash
docker compose up -d --force-recreate ros-humble-dev
docker compose exec ros-humble-dev bash
```

The transform picker and RViz both require a graphical display. If the Compose
service does not declare the X11 socket, start a GUI-capable one-off shell from
the host instead:

```bash
cd /path/to/quadrupled_ws
xhost +local:docker
docker compose run --rm \
  -e DISPLAY="${DISPLAY}" \
  -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
  ros-humble-dev bash
```

If a locally maintained Compose override already passes `DISPLAY` and mounts
`/tmp/.X11-unix`, the regular `docker compose exec` shell can be used instead.

Inside the container:

```bash
export WORKSPACE="$(pwd)"
cd "${WORKSPACE}"
source /opt/ros/humble/setup.bash
colcon build --symlink-install --packages-select spot_navigation
source install/setup.bash
```

Revoke the temporary X11 access on the host after the GUI work is complete:

```bash
xhost -local:docker
```

`--no-show-3d-qc` skips only the final Open3D quality-control window. The
origin picker is still interactive and therefore still requires a display.

## 2. Convert the cloud into a known local origin

The estimator always writes the generic names `transformed.pcd`,
`estimated_transform.json`, and `transform_debug.png` into the current working
directory. Use a separate working directory for each map, then rename the files
so later tools can derive matching names.

The following example uses `dorsett.pcd`. Change only `MAP` and `INPUT` for
`office`, `mcclay`, or `microgrid`.

```bash
cd "${WORKSPACE}"

MAP=dorsett
INPUT="${WORKSPACE}/${MAP}.pcd"
MAP_DIR="${WORKSPACE}/map_work/${MAP}"
TRANSFORM_SCRIPT="${WORKSPACE}/src/spot_navigation/scripts/align_pointcloud_to_map_frame.py"

mkdir -p "${MAP_DIR}"
cd "${MAP_DIR}"

python3 "${TRANSFORM_SCRIPT}" "${INPUT}" \
  --voxel-size 0.05 \
  --ransac-threshold 0.08 \
  --max-lines 16

mv transformed.pcd "${MAP}_transformed.pcd"
mv estimated_transform.json "${MAP}_transform.json"
mv transform_debug.png "${MAP}_transform_debug.png"
```

The three transform parameters shown above are the script defaults and are a
reliable starting point for the existing office, Dorsett, McClay, and microgrid
cloud densities:

| Parameter | Starting value | Purpose |
| --- | ---: | --- |
| `--voxel-size` | `0.05` m | Downsample before line and floor fitting. The saved transformed PCD is also downsampled. |
| `--ransac-threshold` | `0.08` m | Allow normal LiDAR wall thickness/noise while fitting top-down lines. |
| `--max-lines` | `16` | Retain enough dominant wall directions for the interactive view. |

### Click the local frame

1. In the top-down Matplotlib window, click the desired local origin.
2. Click a second point in the desired positive-X direction. The click distance
   does not set scale; only the direction is used.
3. Review the Open3D frame overlay:
   - yellow sphere: selected origin;
   - red: `+X`;
   - green: `+Y`;
   - blue: `+Z`.
4. Close the Open3D window to let the script save its output files.

For the easiest boundary workflow, place the origin on known traversable floor
with at least the intended obstacle inflation clearance around it. This lets
`0, 0` serve as the explicit known-free XY point in the next step. If the map's
semantic origin must instead be on a wall, corner, or machine, record a
different traversable XY coordinate for `--free-point`.

Use the same semantic axis convention across maps where practical. For example,
choose `+X` along the main corridor or building axis rather than allowing each
map to use an arbitrary heading.

> **Screenshot placeholder — `docs/images/map_workflow/01-transform-picker.png`**
> Capture the top-down point cloud after the origin and positive-X clicks. Mark
> the two clicks and label the intended map axes.

> **Screenshot placeholder — `docs/images/map_workflow/02-transform-qc.png`**
> Capture the Open3D quality-control view with the yellow origin and RGB axes.

### Transform acceptance checks

Before continuing, inspect `${MAP}_transform_debug.png` and confirm:

- the selected origin is at the intended physical location;
- red `+X` and green `+Y` follow the intended building directions;
- blue `+Z` points upward;
- the floor lies near `z = 0` in the transformed cloud;
- the transform JSON contains finite values and reports `det_R` close to `1`.

Rerun the estimator and click again if any axis is reversed or the origin is
wrong. Do not compensate for a bad transform while authoring waypoints.

## 3. Generate the boundary map and FAR prior graph

### Choose a known-free point first

An explicit known-free point is the most important robustness setting. It tells
the converter which connected free-space component is traversable and which
side of each boundary FAR should treat as free.

If the transform origin was deliberately placed on open floor, use:

```bash
FREE_X=0.0
FREE_Y=0.0
```

Otherwise, set `FREE_X` and `FREE_Y` to a visually confirmed traversable point
in the transformed frame. The point must lie in a white/free cell after height
filtering and obstacle inflation. Do not omit `--free-point` merely because the
converter can choose one automatically: on maps with a large exterior region,
the automatically selected largest component can be outside the building.

### Shared conservative profile

From the map work directory created above:

```bash
cd "${MAP_DIR}"
BOUNDARY_SCRIPT="${WORKSPACE}/src/spot_navigation/scripts/build_far_prior_map.py"

python3 "${BOUNDARY_SCRIPT}" "${MAP}_transformed.pcd" \
  --output-dir "${MAP_DIR}" \
  --name "${MAP}_transformed" \
  --resolution 0.15 \
  --height-mode local \
  --obstacle-height 0.35 \
  --max-obstacle-height 2.5 \
  --ground-resolution 0.75 \
  --ground-percentile 15 \
  --padding 0.5 \
  --close-radius 0.25 \
  --inflate-radius 0.10 \
  --simplify 0.25 \
  --min-area 0.25 \
  --max-polygons 250 \
  --max-vertices 80 \
  --boundary-z 0.75 \
  --preview-scale 6 \
  --free-point "${FREE_X}" "${FREE_Y}" 0.75
```

This profile uses local height-above-ground instead of absolute Z, making it
less sensitive to small floor-height changes among the four existing sites.
It has been exercised against the current office, Dorsett, McClay, and
microgrid PCD files and generates all five output artifacts without dropping
polygons when `--max-polygons 250` is used. The preview still requires operator
approval; no universal parameter set can distinguish every mapping artifact
from a real obstacle.

### Site starting profiles

Use the shared profile for office, Dorsett, and microgrid. McClay is much larger
and denser, so start with a coarser grid to keep its VGH manageable:

| Site | Resolution | Max polygons | Other settings |
| --- | ---: | ---: | --- |
| Office | `0.15` m | `250` | Shared profile; always set an interior `--free-point`. |
| Dorsett | `0.15` m | `250` | Shared profile. |
| Microgrid | `0.15` m | `250` | Shared profile; always set the intended connected component. |
| McClay | `0.20` m | `250` | Replace only `--resolution 0.15` with `0.20` initially. Return to `0.15` only when narrow passages require it. |

The generated files are:

```text
${MAP}_transformed_boundary.ply
${MAP}_transformed_trajectory.txt
${MAP}_transformed.vgh
${MAP}_transformed_boundary_stats.json
${MAP}_transformed_boundary_preview.png
```

### Read the boundary preview

The PNG uses these colors:

- white: free occupancy cells;
- dark gray: occupied cells after filtering, closing, and inflation;
- green: selected free-space outer boundary;
- red: obstacle polygons inside the selected free component;
- blue: the known-free point.

Accept the boundary only when:

- the blue point is in the intended traversable area;
- the green polygon encloses the intended operating region;
- red polygons follow walls, fixed equipment, and other real obstacles;
- doorways and required corridors remain open with robot clearance;
- the green boundary does not unexpectedly select the outdoor/exterior region;
- `discarded_obstacle_polygons` is `0` in the stats JSON, or every discarded
  polygon is known to be irrelevant.

A green boundary that follows the raster image edge can be valid for an open
site, but it is a warning sign for an indoor map: verify that the known-free
point did not select the exterior component.

> **Screenshot placeholder — `docs/images/map_workflow/03-boundary-preview.png`**
> Insert an accepted `${MAP}_transformed_boundary_preview.png`. Annotate the
> blue free point, green outer boundary, red obstacles, and one open corridor.

### Parameter tuning by symptom

Change one setting at a time, regenerate the preview, and keep the explicit
free point unchanged.

| Symptom | First adjustment | Notes |
| --- | --- | --- |
| Wrong room/exterior component selected | Correct `--free-point X Y Z` | Do this before changing morphology settings. |
| Small gaps break otherwise continuous walls | Increase `--close-radius` from `0.25` to `0.30` or `0.35` | Excessive closing can seal doorways. |
| Required doorway or aisle is blocked | Reduce `--inflate-radius` from `0.10` to `0.05`; then reduce `--close-radius` | Keep a nonzero safety margin unless the runtime stack supplies the full required margin. |
| Low clutter is incorrectly treated as an obstacle | Increase `--obstacle-height` from `0.35` to `0.40` or `0.45` | Confirm Spot can safely pass over or ignore it. |
| Walls/equipment disappear | Reduce `--obstacle-height`, or increase `--max-obstacle-height` if needed | Do not use an absolute-Z workaround until the transformed floor is checked. |
| Sloped/uneven site defeats local ground cells | Try `--height-mode pmf` with the script defaults | PMF is a fallback for varying terrain; compare previews carefully. |
| Too many tiny polygons or an oversized VGH | Increase `--min-area` toward `0.5` and/or `--simplify` toward `0.35` | Never simplify until narrow obstacles and corners have been checked. |
| Warning says polygons were discarded | Increase `--max-polygons` to `400` | A warning means the generated graph omitted smaller candidate obstacles. |
| Narrow passages disappear at McClay | Change `--resolution 0.20` back to `0.15` | Expect a larger VGH and longer generation time. |
| A known driven path is falsely occupied | Use `--clear-trajectory-csv <path>` with a validated XY trajectory and conservative `--clear-radius` | Use only time-aligned, transformed-frame trajectory data; this option intentionally erases occupancy. |

Avoid `--height-mode absolute` as a shared profile. It is appropriate only when
the transformed cloud is known to be level and its absolute Z limits have been
measured for that map.

## 4. Load the map in RViz and record a mission

The mission recorder publishes the transformed PCD, overlays the boundary PLY,
and listens to the standard RViz initial-pose and goal-pose topics.

Inside the built and sourced ROS 2 environment:

```bash
source /opt/ros/humble/setup.bash
source "${WORKSPACE}/install/setup.bash"

ros2 launch spot_navigation mission_recorder.launch.py \
  pcd_file:="${MAP_DIR}/${MAP}_transformed.pcd" \
  boundary_file:="${MAP_DIR}/${MAP}_transformed_boundary.ply" \
  frame_id:=map \
  output_mission_file:="${MAP_DIR}/${MAP}_mission.yaml" \
  output_preview_file:="${MAP_DIR}/${MAP}_mission.png" \
  rviz:=true
```

Because the PCD and boundary follow the same stem and are in the same directory,
`boundary_file` may be omitted. The launch file then resolves it as
`<pcd stem>_boundary.ply`. Paths must be visible inside the environment running
ROS; a host-only path is not valid inside Docker.

In RViz:

1. Confirm `Fixed Frame` is `map`.
2. Confirm the point cloud appears on `/cloud_pcd`.
3. Confirm the boundary overlay appears on `/boundary_map`:
   - green lines are the outer boundary;
   - red lines are obstacle boundaries.
4. Select **2D Pose Estimate**.
5. Click and drag once on free space to set the robot's initial position and
   heading.
6. Select **2D Goal Pose**.
7. Click and drag each waypoint in route order. Arrow direction records the
   desired orientation at that waypoint.
8. Watch the launch terminal for `Captured initial pose` and `Captured waypoint`
   messages after every click.
9. Press `Ctrl+C` in the launch terminal when finished.

The recorder writes output only when it has one initial pose and at least one
waypoint. It writes the YAML and top-down PNG after shutdown. It does not enforce
boundary collisions, so every clicked pose must be visually checked against the
cloud and red boundary lines.

The recorder has no undo command. If a pose is clicked incorrectly, stop the
session, relaunch it, and record the route again rather than editing an unknown
quaternion by hand.

> **Screenshot placeholder — `docs/images/map_workflow/04-rviz-map-loaded.png`**
> Capture RViz in top-down view with the transformed PCD, map origin axes, green
> outer boundary, and red obstacle boundaries visible.

> **Screenshot placeholder — `docs/images/map_workflow/05-rviz-initial-pose.png`**
> Capture the **2D Pose Estimate** arrow on valid free space and show the
> corresponding `Captured initial pose` terminal message.

> **Screenshot placeholder — `docs/images/map_workflow/06-rviz-waypoints.png`**
> Capture several **2D Goal Pose** clicks in route order. Add waypoint numbers in
> the edited documentation image if RViz itself does not show them.

> **Screenshot placeholder — `docs/images/map_workflow/07-mission-preview.png`**
> Insert the generated `${MAP}_mission.png`, showing the start, ordered
> waypoints, headings, and route order.

### Mission acceptance checks

Open `${MAP}_mission.png` and `${MAP}_mission.yaml` and confirm:

- the start marker is in free space and has the correct heading;
- all waypoints are inside the intended green outer boundary;
- no waypoint lies inside or immediately against a red obstacle polygon;
- route segments do not require crossing walls or closed boundaries;
- waypoint order and headings match the intended traversal;
- `frame_id` is `map`;
- the YAML contains exactly one `initial_pose` and the expected number of
  `waypoints`.

## 5. Use the resulting map and mission with FAR Planner

Start localization with the same transformed PCD:

```bash
ros2 launch spot_navigation lio_localization.launch.py \
  map_path:="${MAP_DIR}/${MAP}_transformed.pcd" \
  use_sim_time:=false
```

In another sourced terminal, load the generated VGH and optionally start the
route manager with the recorded mission:

```bash
ros2 launch spot_navigation far_planner.launch.py \
  use_sim_time:=false \
  load_prior_map:=true \
  prior_map_path:="${MAP_DIR}/${MAP}_transformed.vgh" \
  route_manager:=true \
  mission_file:="${MAP_DIR}/${MAP}_mission.yaml" \
  rviz:=true
```

The localization PCD, boundary PLY, VGH, and mission YAML must all come from the
same transformed frame. Never combine waypoints authored against one transform
with a PCD or VGH generated from another transform-estimator run.

## Troubleshooting

### `Open3D is required`

Rebuild the project image so the version declared in the Dockerfile is installed:

```bash
docker compose build ros-humble-dev
docker compose run --rm ros-humble-dev \
  python3 -c "import open3d; print(open3d.__version__)"
```

### No picker window appears

Verify `DISPLAY`, X11/Wayland forwarding, and container GUI access. The script
cannot choose the semantic origin without an interactive Matplotlib click.

### `Not enough lines detected`

Try the following in order:

1. Increase `--ransac-threshold` from `0.08` to `0.10`.
2. Increase `--voxel-size` from `0.05` to `0.08` for a very dense/noisy cloud.
3. Increase `--max-lines` from `16` to `24` if many short wall sections exist.

If the top-down cloud has no meaningful wall-like geometry, this simplified
estimator is not the appropriate registration tool.

### `requested free point lies on an occupied cell`

The XY point is outside free occupancy after filtering and inflation. Choose a
nearby visually verified open point. If `(0, 0)` was intended to be free, first
recheck that the transform origin was clicked on open floor.

### Boundary preview selects the exterior

Rerun `build_far_prior_map.py` with an explicit point inside the intended room,
corridor, or yard. Do not tune `--close-radius` merely to force a different
component.

### Boundary is not visible in RViz

Check all of the following:

- the PLY is ASCII and was produced by `build_far_prior_map.py`;
- `boundary_file` points to the container-visible file;
- the RViz `Boundary Map` display subscribes to `/boundary_map`;
- the display uses transient-local, reliable QoS;
- `frame_id` and RViz Fixed Frame are both `map`.

### Mission files are not written

The launch terminal must receive `Ctrl+C`, and the recorder must have captured
one initial pose plus at least one goal pose. Check the terminal for rejected
poses or frame mismatch errors.

## Artifact handling

The workspace's top-level `.gitignore` ignores new `*.pcd` files because they
are large, but it does not automatically ignore every generated PLY, VGH, JSON,
PNG, TXT, or YAML. Keep experimental output under `map_work/`. After review,
copy only the approved deployment artifacts into the intended map location.
Do not force-add large PCD maps or commit generated site data unless the
repository's data/versioning policy explicitly calls for it.
