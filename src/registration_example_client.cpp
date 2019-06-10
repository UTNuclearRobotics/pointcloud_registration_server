
#ifndef REGISTRATION_EXAMPLE_CLIENT
#define REGISTRATION_EXAMPLE_CLIENT

#include <ros/ros.h>
#include "pointcloud_registration_server/pointcloud_registration.h"
#include "pointcloud_registration_server/registration_service.h"
#include "pointcloud_registration_server/reg_creation.h"
#include <rosbag/bag.h>
#include <rosbag/view.h>

// to introduce error
#include <geometry_msgs/TransformStamped.h>
#include <tf2/transform_datatypes.h>
#include <tf2_sensor_msgs/tf2_sensor_msgs.h>
#include <manual_pointcloud_alignment/manual_pointcloud_alignment.h>
//#include <tf/Quaternion.h>

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

sensor_msgs::PointCloud2 first_cloud;
sensor_msgs::PointCloud2 second_cloud;
bool first_cloud_acquired;
bool second_cloud_acquired;

void pointcloudCallback(sensor_msgs::PointCloud2 input)
{
	ROS_INFO_STREAM(first_cloud_acquired << " " << second_cloud_acquired);
	if(!first_cloud_acquired)
	{
		first_cloud_acquired = true;
		first_cloud = input;	
	}
	else if(!second_cloud_acquired)
	{
		second_cloud_acquired = true;
		second_cloud = input;
	}
	else 
		ROS_ERROR_STREAM("[RegistrationClient] Pointcloud callback received even though both clouds are already found! Exiting callback...");
}

int main (int argc, char **argv)
{ 
  	if( ros::console::set_logger_level(ROSCONSOLE_DEFAULT_NAME, ros::console::levels::Debug) )
    	ros::console::notifyLoggerLevelsChanged();  

	ros::init(argc, argv, "registration_example");

	ros::NodeHandle nh;

	first_cloud_acquired = false;
	second_cloud_acquired = false;

	// Load parameters from server
	std::string topic;
	float initial_pause;	
	float betwixt_pause;
	nh.param<std::string>("registration_example/topic", topic, "camera/depth/points");
	nh.param<float>("registration_example/initial_pause", initial_pause, 3.0);
	nh.param<float>("registration_example/betwixt_pause", betwixt_pause, 3.0); 

	bool load_from_bags;
	std::string bag_name_1;
	std::string bag_name_2; 
	nh.param<bool>("registration_example/load_cloud", load_from_bags, false);
	if(load_from_bags)
		nh.param<std::string>("registration_example/bag_topic", topic, "laser_mapper/local_dense_cloud");
	nh.param<std::string>("registration_example/bag_name_1", bag_name_1, "./cloud_1.bag");
	nh.param<std::string>("registration_example/bag_name_2", bag_name_2, "./cloud_2.bag");

	std::vector<float> introduced_error;
	if( !nh.getParam("registration_example/introduced_error", introduced_error) )
		for(int i=0; i<3; i++)
			introduced_error.push_back(0.0);

	ros::Subscriber topic_sub = nh.subscribe<sensor_msgs::PointCloud2>(topic, 1, pointcloudCallback);
	ros::Publisher first_cloud_pub = nh.advertise<sensor_msgs::PointCloud2>("registration_example/first_cloud", 1);
	ros::Publisher second_cloud_preprocessed_pub = nh.advertise<sensor_msgs::PointCloud2>("registration_example/second_cloud_preprocessed", 1);
	ros::Publisher first_cloud_preprocessed_pub = nh.advertise<sensor_msgs::PointCloud2>("registration_example/first_cloud_preprocessed", 1);
	ros::Publisher second_cloud_pub = nh.advertise<sensor_msgs::PointCloud2>("registration_example/second_cloud", 1);
	ros::Publisher final_cloud_pub = nh.advertise<sensor_msgs::PointCloud2>("registration_example/final_cloud", 1);
	ros::ServiceClient client = nh.serviceClient<pointcloud_registration_server::registration_service>("pointcloud_registration");
	
	if(!load_from_bags)
	{
		ROS_INFO_STREAM("[RegistrationClient] Client started! Pausing for " << initial_pause << " seconds to allow camera to be positioned and start up.");
		ros::Duration(initial_pause).sleep();
		// Acquire first cloud
		while(ros::ok() && !first_cloud_acquired)
		{
			ROS_INFO_STREAM("spinning first spot");
			ros::spinOnce();
			ros::Duration(0.9).sleep();
		}
		first_cloud_pub.publish(first_cloud);

		ROS_INFO_STREAM("[RegistrationClient] First cloud recorded, containing " << first_cloud.height*first_cloud.width << " points! Pausing for " << betwixt_pause << " seconds to allow camera to be positioned for second cloud.");
		ros::Duration(betwixt_pause).sleep();

		// Acquire second cloud
		while(ros::ok() && !second_cloud_acquired)
		{
			ROS_INFO_STREAM("spinning second spot");
			ros::spinOnce();
			ros::Duration(0.1).sleep();
		}
		second_cloud_pub.publish(second_cloud);

		ROS_INFO_STREAM("[RegistrationClient] Second cloud recorded, containing " << second_cloud.height*second_cloud.width << " points! Starting to resize clouds.");
	}
	else
	{
		ROS_INFO_STREAM("[RegistrationClient] Loading clouds from bag files, using bag names: " << bag_name_1 << " and " << bag_name_2 << " and topic name " << topic << ".");
		rosbag::Bag bag_1; 
		bag_1.open(bag_name_1, rosbag::bagmode::Read);

		std::vector<std::string> topics;
		topics.push_back(topic);
		//topics.push_back("/rosout");
		rosbag::View view_1(bag_1, rosbag::TopicQuery(topics));

		BOOST_FOREACH(rosbag::MessageInstance const m, view_1)
	    {
	    	ROS_INFO_STREAM("actually entering the loop");
	        sensor_msgs::PointCloud2::ConstPtr cloud_1_ptr = m.instantiate<sensor_msgs::PointCloud2>();
	        if (cloud_1_ptr != NULL)
	            first_cloud = *cloud_1_ptr;
	        else
	        	ROS_ERROR_STREAM("[RegistrationClient] Cloud caught for first cloud is null...");
	    }
	    bag_1.close();

	    rosbag::Bag bag_2; 
		bag_2.open(bag_name_2, rosbag::bagmode::Read);

		rosbag::View view_2(bag_2, rosbag::TopicQuery(topics));

		foreach(rosbag::MessageInstance const m, view_2)
	    {
	        sensor_msgs::PointCloud2::ConstPtr cloud_2_ptr = m.instantiate<sensor_msgs::PointCloud2>();
	        if (cloud_2_ptr != NULL)
	            second_cloud = *cloud_2_ptr;
	        else
	        	ROS_ERROR_STREAM("[RegistrationClient] Cloud caught for second cloud is null...");
	    }
	    bag_2.close();

	    ROS_INFO_STREAM("[RegistrationClient] Both clouds collected from bag files - cloud sizes are " << first_cloud.height*first_cloud.width << " and " << second_cloud.height*second_cloud.width << " points, respectively.");
	}

	ROS_INFO_STREAM("[RegistrationClient] Introducing specified error:");
	ROS_INFO_STREAM("   Translation: " << introduced_error[0] << " " << introduced_error[1] << " " << introduced_error[2]);
	ROS_INFO_STREAM("   Rotation:    " << introduced_error[3] << " " << introduced_error[4] << " " << introduced_error[5]);
	tf::Quaternion quat_tf = tf::createQuaternionFromRPY(introduced_error[3], introduced_error[4], introduced_error[5]);
	geometry_msgs::TransformStamped error_transform; 
	error_transform.header = second_cloud.header;
	error_transform.transform.rotation.x = quat_tf.getAxis().getX()*sin(quat_tf.getAngle()/2);
	error_transform.transform.rotation.y = quat_tf.getAxis().getY()*sin(quat_tf.getAngle()/2);
	error_transform.transform.rotation.z = quat_tf.getAxis().getZ()*sin(quat_tf.getAngle()/2);
	error_transform.transform.rotation.w = cos(quat_tf.getAngle()/2);
	error_transform.transform.translation.x = introduced_error[0];
	error_transform.transform.translation.y = introduced_error[1];
	error_transform.transform.translation.z = introduced_error[2];
	// ----------- Performing Rotation -----------
	sensor_msgs::PointCloud2 second_cloud_transformed;
	tf2::doTransform (second_cloud, second_cloud_transformed, error_transform);

	first_cloud_pub.publish(first_cloud);
	second_cloud_pub.publish(second_cloud_transformed);


	ROS_INFO_STREAM("[RegistrationClient] Interposing manual error alignment.");
	ros::ServiceClient manual_alignment_server = nh.serviceClient<manual_pointcloud_alignment::manual_alignment_service>("manual_pointcloud_alignment/align_clouds");
	manual_pointcloud_alignment::manual_alignment_service alignment_service;
	alignment_service.request.fixed_cloud = first_cloud;
	alignment_service.request.cloud_to_align = second_cloud_transformed;
	while(ros::ok() && !manual_alignment_server.call(alignment_service))
	{
		ROS_ERROR_STREAM("[RegistrationClient] Attempt to call alignment service failed... prob not up yet. Waiting and trying again.");
		ros::Duration(0.5).sleep();
	}
	ROS_INFO_STREAM("[RegistrationClient] Final manual transform: " << alignment_service.response.transform);


	// Run the registration service
	pointcloud_registration_server::registration_service reg_srv;
	reg_srv.request.cloud_list.push_back(first_cloud);
	reg_srv.request.cloud_list.push_back(alignment_service.response.aligned_cloud);

	RegCreation::registrationFromYAML(&reg_srv, "registration_example");
	while(ros::ok() && !client.call(reg_srv))
	{
		ROS_ERROR_STREAM("[RegistrationClient] Attempted service call but returned false. Probably not up. Restart the service node!");
		ros::Duration(1.0).sleep();
	}

	ROS_INFO_STREAM("[RegistrationClient] Successfully finished registration call! Time costs: " );
	//ROS_INFO_STREAM("    1st Preprocessing: " << reg_srv.response.preprocessing_time[0] << "  2nd Preprocessing: " << reg_srv.response.preprocessing_time[1] << "  Registration: " << reg_srv.response.registration_time[0]);

	while(ros::ok())
	{
		first_cloud_pub.publish(first_cloud);
		second_cloud_pub.publish(second_cloud_transformed);
		first_cloud_preprocessed_pub.publish(reg_srv.response.preprocessing_results[0].task_pointcloud);
		second_cloud_preprocessed_pub.publish(reg_srv.response.preprocessing_results[1].task_pointcloud);
		final_cloud_pub.publish(reg_srv.response.full_cloud);
		ros::Duration(0.5).sleep();
	}

	return 0;
}

#endif // REGISTRATION_EXAMPLE_CLIENT