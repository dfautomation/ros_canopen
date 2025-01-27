/*
 * Copyright (c) 2016, Ivor Wanders
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ros/ros.h>
#include <socketcan_bridge/topic_to_socketcan.h>
#include <socketcan_bridge/socketcan_to_topic.h>
#include <socketcan_interface/threading.h>
#include <socketcan_interface/xmlrpc_settings.h>
#include <memory>
#include <string>
#include <diagnostic_updater/diagnostic_updater.h>
#include <diagnostic_updater/update_functions.h>

boost::shared_ptr<diagnostic_updater::Updater> diagnostic_updater_;

// diagnostic variable
bool can_connected_;
int write_count_;
int read_count_;

void init();
void diagnosticStatus(diagnostic_updater::DiagnosticStatusWrapper &stat);
void callback(socketcan_bridge::TopicToSocketCAN *W, socketcan_bridge::SocketCANToTopic *R);

void init()
{
  can_connected_ = true;
  write_count_ = 0;
  read_count_ = 0;
}

void diagnosticStatus(diagnostic_updater::DiagnosticStatusWrapper &stat)
{
  stat.add("Writes/sec", write_count_);
  stat.add("Reads/sec", read_count_);

  if (can_connected_)
  {
    stat.summary(diagnostic_msgs::DiagnosticStatus::OK, "OK");
  } else {
    stat.summary(diagnostic_msgs::DiagnosticStatus::ERROR, "CAN disconnected");
  }
}

void callback(socketcan_bridge::TopicToSocketCAN *W, socketcan_bridge::SocketCANToTopic *R)
{
  write_count_ = W->get_write_count();
  read_count_ = R->get_read_count();
  if (W->get_connection_error() || R->get_connection_error())
  {
    can_connected_ = false;
  }
  diagnostic_updater_->update();
}

int main(int argc, char *argv[])
{
  ros::init(argc, argv, "socketcan_bridge_node");

  init();

  // diagnostic updater
  diagnostic_updater_.reset(new diagnostic_updater::Updater());
  diagnostic_updater_->setHardwareID("AGV");
  diagnostic_updater_->add("Status", diagnosticStatus);

  ros::NodeHandle nh(""), nh_param("~");

  std::string can_device;
  nh_param.param<std::string>("can_device", can_device, "can0");

  can::ThreadedSocketCANInterfaceSharedPtr driver = std::make_shared<can::ThreadedSocketCANInterface> ();

  // initialize device at can_device, 0 for no loopback.
  if (!driver->init(can_device, 0, XmlRpcSettings::create(nh_param)))
  {
    ROS_FATAL("Failed to initialize can_device at %s", can_device.c_str());
    return 1;
  }
    else
  {
    ROS_INFO("Successfully connected to %s.", can_device.c_str());
  }

  // initialize the bridge both ways.
  socketcan_bridge::TopicToSocketCAN to_socketcan_bridge(&nh, &nh_param, driver);
  to_socketcan_bridge.setup();

  socketcan_bridge::SocketCANToTopic to_topic_bridge(&nh, &nh_param, driver);
  to_topic_bridge.setup(nh_param);

  ros::Timer timer = nh.createTimer(ros::Duration(1.0),
                    boost::bind(&callback, &to_socketcan_bridge, &to_topic_bridge),
                    false);

  ros::spin();

  driver->shutdown();
  driver.reset();

  ros::waitForShutdown();
}
