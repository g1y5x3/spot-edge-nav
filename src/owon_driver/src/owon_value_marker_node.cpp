#include <cmath>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include "geometry_msgs/msg/point.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32.hpp"
#include "visualization_msgs/msg/marker.hpp"

class OwonValueMarkerNode : public rclcpp::Node
{
public:
    OwonValueMarkerNode() : Node("owon_value_marker_node")
    {
        this->declare_parameter("value_topic", "/owon/value");
        this->declare_parameter("marker_topic", "/owon/voltage_marker");
        this->declare_parameter("marker_frame_id", "base_link");
        this->declare_parameter("marker_namespace", "owon_voltage");
        this->declare_parameter("marker_id", 0);
        this->declare_parameter("marker_x", 0.0);
        this->declare_parameter("marker_y", 0.0);
        this->declare_parameter("marker_z", 2.0);
        this->declare_parameter("marker_text_size", 1.0);
        this->declare_parameter("marker_lifetime_sec", 3.0);
        this->declare_parameter("stamp_with_current_time", false);
        this->declare_parameter("text_prefix", "");
        this->declare_parameter("text_suffix", " V DC");
        this->declare_parameter("text_precision", 2);
        this->declare_parameter("text_color_mode", "fixed");  // fixed or threshold
        this->declare_parameter("text_color_r", 1.0);
        this->declare_parameter("text_color_g", 1.0);
        this->declare_parameter("text_color_b", 1.0);
        this->declare_parameter("enable_indicator_line", true);
        this->declare_parameter("indicator_line_width", 0.05);
        this->declare_parameter("indicator_line_color_r", 0.0);
        this->declare_parameter("indicator_line_color_g", 1.0);
        this->declare_parameter("indicator_line_color_b", 1.0);
        this->declare_parameter("indicator_line_alpha", 0.9);
        this->declare_parameter("voltage_warn_threshold", 45.0);
        this->declare_parameter("voltage_critical_threshold", 42.0);

        value_topic_ = this->get_parameter("value_topic").as_string();
        marker_topic_ = this->get_parameter("marker_topic").as_string();
        marker_frame_id_ = this->get_parameter("marker_frame_id").as_string();
        marker_namespace_ = this->get_parameter("marker_namespace").as_string();
        marker_id_ = static_cast<int32_t>(this->get_parameter("marker_id").as_int());
        marker_x_ = this->get_parameter("marker_x").as_double();
        marker_y_ = this->get_parameter("marker_y").as_double();
        marker_z_ = this->get_parameter("marker_z").as_double();
        marker_text_size_ = this->get_parameter("marker_text_size").as_double();
        marker_lifetime_sec_ = this->get_parameter("marker_lifetime_sec").as_double();
        stamp_with_current_time_ = this->get_parameter("stamp_with_current_time").as_bool();
        text_prefix_ = this->get_parameter("text_prefix").as_string();
        text_suffix_ = this->get_parameter("text_suffix").as_string();
        text_precision_ = static_cast<int>(this->get_parameter("text_precision").as_int());
        text_color_mode_ = this->get_parameter("text_color_mode").as_string();
        text_color_r_ = this->get_parameter("text_color_r").as_double();
        text_color_g_ = this->get_parameter("text_color_g").as_double();
        text_color_b_ = this->get_parameter("text_color_b").as_double();
        enable_indicator_line_ = this->get_parameter("enable_indicator_line").as_bool();
        indicator_line_width_ = this->get_parameter("indicator_line_width").as_double();
        indicator_line_color_r_ = this->get_parameter("indicator_line_color_r").as_double();
        indicator_line_color_g_ = this->get_parameter("indicator_line_color_g").as_double();
        indicator_line_color_b_ = this->get_parameter("indicator_line_color_b").as_double();
        indicator_line_alpha_ = this->get_parameter("indicator_line_alpha").as_double();
        voltage_warn_threshold_ = this->get_parameter("voltage_warn_threshold").as_double();
        voltage_critical_threshold_ = this->get_parameter("voltage_critical_threshold").as_double();

        marker_pub_ = this->create_publisher<visualization_msgs::msg::Marker>(
            marker_topic_, rclcpp::QoS(rclcpp::KeepLast(10)).reliable());
        value_sub_ = this->create_subscription<std_msgs::msg::Float32>(
            value_topic_,
            rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
            std::bind(&OwonValueMarkerNode::value_callback, this, std::placeholders::_1));

        RCLCPP_INFO(
            this->get_logger(),
            "Publishing voltage markers from %s to %s in frame %s (size %.2f, z %.2f)",
            value_topic_.c_str(), marker_topic_.c_str(), marker_frame_id_.c_str(),
            marker_text_size_, marker_z_);
    }

private:
    void value_callback(const std_msgs::msg::Float32::SharedPtr msg)
    {
        visualization_msgs::msg::Marker marker;
        marker.header.frame_id = marker_frame_id_;
        if (stamp_with_current_time_) {
            marker.header.stamp = this->now();
        }
        marker.ns = marker_namespace_;
        marker.id = marker_id_;
        marker.type = visualization_msgs::msg::Marker::TEXT_VIEW_FACING;
        marker.action = visualization_msgs::msg::Marker::ADD;
        marker.pose.position.x = marker_x_;
        marker.pose.position.y = marker_y_;
        marker.pose.position.z = marker_z_;
        marker.pose.orientation.w = 1.0;
        marker.scale.z = marker_text_size_;
        marker.text = format_voltage(msg->data);
        marker.lifetime = rclcpp::Duration::from_seconds(marker_lifetime_sec_);

        set_text_color(msg->data, marker);
        if (enable_indicator_line_) {
            publish_indicator_line(marker);
        }
        marker_pub_->publish(marker);
    }

    std::string format_voltage(float value) const
    {
        std::ostringstream stream;
        stream << text_prefix_;
        if (std::isfinite(value)) {
            stream << std::fixed << std::setprecision(text_precision_) << value;
        } else {
            stream << "nan";
        }
        stream << text_suffix_;
        return stream.str();
    }

    void set_text_color(float value, visualization_msgs::msg::Marker& marker) const
    {
        marker.color.a = 1.0;
        if (text_color_mode_ == "threshold") {
            if (!std::isfinite(value)) {
                marker.color.r = 1.0;
                marker.color.g = 1.0;
                marker.color.b = 1.0;
            } else if (value <= voltage_critical_threshold_) {
                marker.color.r = 1.0;
                marker.color.g = 0.1;
                marker.color.b = 0.1;
            } else if (value <= voltage_warn_threshold_) {
                marker.color.r = 1.0;
                marker.color.g = 0.8;
                marker.color.b = 0.0;
            } else {
                marker.color.r = 0.1;
                marker.color.g = 1.0;
                marker.color.b = 0.2;
            }
            return;
        }

        marker.color.r = text_color_r_;
        marker.color.g = text_color_g_;
        marker.color.b = text_color_b_;
    }

    void publish_indicator_line(const visualization_msgs::msg::Marker& text_marker) const
    {
        visualization_msgs::msg::Marker line;
        line.header = text_marker.header;
        line.ns = marker_namespace_;
        line.id = marker_id_ + 100;
        line.type = visualization_msgs::msg::Marker::LINE_STRIP;
        line.action = visualization_msgs::msg::Marker::ADD;
        line.pose.orientation.w = 1.0;
        line.scale.x = indicator_line_width_;
        line.color.r = indicator_line_color_r_;
        line.color.g = indicator_line_color_g_;
        line.color.b = indicator_line_color_b_;
        line.color.a = indicator_line_alpha_;
        line.lifetime = rclcpp::Duration::from_seconds(marker_lifetime_sec_);

        geometry_msgs::msg::Point start;
        start.x = 0.0;
        start.y = 0.0;
        start.z = 0.0;
        geometry_msgs::msg::Point end;
        end.x = text_marker.pose.position.x;
        end.y = text_marker.pose.position.y;
        end.z = text_marker.pose.position.z;
        line.points.push_back(start);
        line.points.push_back(end);

        marker_pub_->publish(line);
    }

    std::string value_topic_;
    std::string marker_topic_;
    std::string marker_frame_id_;
    std::string marker_namespace_;
    int32_t marker_id_;
    double marker_x_;
    double marker_y_;
    double marker_z_;
    double marker_text_size_;
    double marker_lifetime_sec_;
    bool stamp_with_current_time_;
    std::string text_prefix_;
    std::string text_suffix_;
    int text_precision_;
    std::string text_color_mode_;
    double text_color_r_;
    double text_color_g_;
    double text_color_b_;
    bool enable_indicator_line_;
    double indicator_line_width_;
    double indicator_line_color_r_;
    double indicator_line_color_g_;
    double indicator_line_color_b_;
    double indicator_line_alpha_;
    double voltage_warn_threshold_;
    double voltage_critical_threshold_;

    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr marker_pub_;
    rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr value_sub_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<OwonValueMarkerNode>());
    rclcpp::shutdown();
    return 0;
}
