#include "rrt.h"
#include <iostream>
#include "vision_detection.pb.h"
#include <string.h>
#include <fstream>
#include <vector>
#include "udp_commu_recv.h"
#include "udp_commu_send.h"
#include <math.h>
#include <ctime>

using namespace std;
#pragma comment(lib, "WS2_32.lib")

int test_car_number = 4;

int EXPAND = 20;
int Max_vel = 400;
int expand;
//��ͼ���ٶȿ��Ʋ���
const double car_radius = 90;
const double ball_radius = 50;

const double resolution = 0.1;
int map_cost[450][600];
int map_blue_obstacles_x[8];
int map_blue_obstacles_y[8];
int map_yellow_obstacles_x[8];
int map_yellow_obstacles_y[8];

int number_map_blue_obstacles = 0;
int number_map_yellow_obstacles = 0;
double x_new_path[100];
double y_new_path[100];
int new_point_num;

double offset = 7;
double lethal_distance = 40.0;

//�ڷ��������·��
void setpath(Debug_Msgs lines_set, int* x_path, int* y_path, int number) {
	for (int i = 0; i < number; i++)
	{
		Debug_Msg* line_set = new Debug_Msg();
		line_set = lines_set.add_msgs();
		line_set->set_type(Debug_Msg_Debug_Type_LINE);
		line_set->set_color(Debug_Msg_Color_BLUE);
		Debug_Line* line = new Debug_Line();
		Point* start = new Point();
		Point* end = new Point();

		start->set_x(x_path[i] - 300);
		start->set_y(y_path[i] - 225);
		end->set_x(x_path[i + 1] - 300);
		end->set_y(y_path[i + 1] - 225);

		line->set_allocated_start(start);
		line->set_allocated_end(end);
		line->set_forward(true);
		line->set_back(true);
		line_set->set_allocated_line(line);
	}

	char* buffer = new char[10240];
	int buffer_size = 10240;
	lines_set.SerializeToArray(buffer, buffer_size);

	WSADATA wsaData;
	sockaddr_in RecvAddr;
	int Port = 20001;
	SOCKET Socket;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	cout << sendto(Socket, buffer, buffer_size, 0, (SOCKADDR*)& RecvAddr, sizeof(RecvAddr)) << endl;
}   

// �������֮��б��
double Get_angle(int x1, int y1, int x2, int y2) {
	
	double angle;
	if (x2 - x1 < 0.01)
		angle = 10000;
	else
		angle = (y2 - y1) / (x2 - x1);
	return angle;
}

// �������֮�����
int Get_distance(int x1, int y1, int x2, int y2) {
	int distance = sqrt((x2 - x1)*(x2 - x1) + (y2 - y1)*(y2 - y1));
	return distance;
}

// ��õ㵽ֱ�߾���
double Get_dis_point_to_line(int cur_x, int cur_y, int end_x, int end_y, float alpha) {
	if (abs(alpha - 3.14159 / 2) < 0.1)
		return abs(cur_x - end_x);
	else {
		float k = tan(alpha);
		float b = end_y - k * end_x;
		float dis = abs(k*cur_x - cur_y + b) / sqrt(k*k + 1);
		return dis;
	}
}

// ��ƽ�����·���Ͻ��в�㣬ÿһ��·���������ȷ֣��õ��µ�·�� x_new_path / y_new_path
void Get_new_path(int *path_x, int *path_y, int point_num) {
	new_point_num = 0;

	double dx, dy;
	for (int i = 0; i < point_num - 1; i++) {
		dx = (path_x[i + 1] - path_x[i]) / 3.0;
		dy = (path_y[i + 1] - path_y[i]) / 3.0;
		for (int k = 0; k < 3; k++) {
			x_new_path[new_point_num] = path_x[i] + k * dx;
			y_new_path[new_point_num] = path_y[i] + k * dy;
			new_point_num++;
		}
	}
	x_new_path[new_point_num] = path_x[point_num - 1];
	y_new_path[new_point_num] = path_y[point_num - 1];
	new_point_num++;
}

// �����ٶ�  point��ÿһ���ڵ���Ϣ������·������  start/goal ��¼ת�۵���Ϣ�������ٶȷ���  ���� Ϊ�˼��ٲ����������ٶȵ�Ӱ��
double* setv(double cur_x, double cur_y, double point_x, double point_y, double start_x, double start_y, double goal_x, double goal_y, double oritation, int flag)
{
	double *vel = new double[2];
	float check = 200;
	int acc_down = 50;
	int speed;

	int dx = point_x - cur_x;
	int dy = point_y - cur_y;
	float alpha = atan2(dy, dx);    //·���н� - ��ǰ�ڵ�ָ����һ�ڵ�

	float temp_vx;
	float temp_vy;
	float dv_x, dv_y;

	//double temp_dis1 = Get_distance(cur_x, cur_y, start_x, start_y);
	double temp_dis = Get_distance(cur_x, cur_y, goal_x, goal_y);   //��ǰλ�õ���һ�ڵ����
	double distance = Get_distance(start_x, start_y, goal_x, goal_y); // ÿһ��·���ĳ���

	if (flag == 1) {   //·�����һ�Σ�ȷ�����ȶ�ͣ���յ�
		if (temp_dis > distance / 2) {
			speed = sqrt(800 * abs(distance - temp_dis) + 2500);  //�� 80 �ľ����ڽ��м���
			if (distance - temp_dis > 80)
				speed = 380;                                 //��һ����Χ������
		}
		else {
			speed = sqrt(60 * temp_dis) + 30;                 // ������ ����ʵ����������Ƽ��پ���
			if (temp_dis > 80)
				speed = 380;
		}
	}
	
	if (flag == 0) {    //·���м�Σ�����һ���ٶ�
		if (temp_dis > distance / 2) {
			speed = sqrt(800 * abs(distance - temp_dis)) + 50;
			if (distance - temp_dis > 80)
				speed = 380;
		}
		else {
			speed = 70 + sqrt(60 * temp_dis);
			if (temp_dis > 80)
				speed = 380;
		}
	}

	temp_vx = cos(alpha - oritation) * speed;   // ת������ϵ
	temp_vy = sin(alpha - oritation) * speed;    

	float cur_dis = Get_dis_point_to_line(cur_x, cur_y, goal_x, goal_y, alpha);
	double alpha_1 = atan2(cur_y - start_y, cur_x - start_x);
	int cof;
	if (alpha_1 > alpha) {
		cof = 1;
	}
	else if (alpha_1 < alpha) {
		cof = -1;
	}
	else {
		cof = 0;
	}
	dv_x = cof * check * cur_dis * sin(alpha - oritation);       // �����ٶ�����ֵ ����ƫ��·����������ȣ�check���������ӣ�������С
	dv_y = -cof * check * cur_dis * cos(alpha - oritation);

	vel[0] = temp_vx + dv_x; 
	vel[1] = temp_vy + dv_y;
	return vel;
}

// ��õ�ͼ
void get_map(udp_commu_recv udp_recv)
{
	udp_recv.Recv(1);

	for (int i = 0; i < 450; i++)
	{
		for (int j = 0; j < 600; j++)
		{
			//	map_cost[i][j] = 0;
		}
	}
	number_map_blue_obstacles = udp_recv.vision_recv.robots_blue_size();
	for (int i = 0; i < udp_recv.vision_recv.robots_blue_size(); i++)
	{
		//udp_recv.Recv();
		if (udp_recv.robot_blue[i].id != test_car_number)
		{

			int x = floor(udp_recv.robot_blue[i].x * resolution);
			int y = floor(udp_recv.robot_blue[i].y * resolution);
			map_blue_obstacles_x[i] = x + 300;
			map_blue_obstacles_y[i] = y + 225;
			
			for (int i = int(x - car_radius * resolution - expand); i < int(x + car_radius * resolution + expand); i++)
			{
				for (int j = int(y - car_radius * resolution - expand); j < int(y + car_radius * resolution + expand); j++)
				{
					//			map_cost[j + 225][i + 300] = 1;
				}
			}
		}
	
	}
	for (int i = number_map_blue_obstacles; i < 8; i++)
	{
		map_blue_obstacles_x[i] = -1;
		map_blue_obstacles_y[i] = -1;
	}

	number_map_yellow_obstacles = udp_recv.vision_recv.robots_yellow_size();
	for (int i = 0; i < udp_recv.vision_recv.robots_yellow_size(); i++)
	{

		//udp_recv.Recv();
		int x = floor(udp_recv.robot_yellow[i].x * resolution);
		int y = floor(udp_recv.robot_yellow[i].y * resolution);
		map_yellow_obstacles_x[i] = x + 300;
		map_yellow_obstacles_y[i] = y + 225;
		
		for (int i = int(x - car_radius * resolution - expand); i < int(x + car_radius * resolution + expand); i++)
		{
			for (int j = int(y - car_radius * resolution - expand); j < int(y + car_radius * resolution + expand); j++)
			{
				map_cost[j + 225][i + 300] = 1;
			}
		}
	}
	for (int i = number_map_yellow_obstacles; i < 8; i++)
	{
		map_yellow_obstacles_x[i] = -1;
		map_yellow_obstacles_y[i] = -1;
	}

}

int main() {
	/*********************************************************************/
	udp_commu_recv udp_recv;
	udp_recv.my_id = test_car_number;
	udp_recv.Recv(1);
	udp_commu_send udp_send;
	udp_send.init();

	// ��ʼ������
	int pointnumber_smooth = 0;   //��¼·������
	int check_num = 0;
	int x_path_smooth[600] = { 0 };  // ��¼·����������Ϣ
	int y_path_smooth[450] = { 0 };
	int* tempx;
	int* tempy;
	int flag = 0;
	int x = int(udp_recv.my_robot.x);
	int y = int(udp_recv.my_robot.y);
	cout << x << " " << y << endl;
	int goal_x = 50;               // ��� �յ�λ�ò���
	int goal_y = 375;
	int start_x = 550;
	int start_y = 75;
	int temp_x;
	int temp_y;
	int nearest;
	int fail_time = 1;

	RrtPath * rrtpath = new RrtPath;
	while (1)
	{
		if (fail_time == 0) {      // ���ǹ滮·��ʧ�ܵ������ �������ʹ�С��ʹ��˳������
			expand = 3;
			(*rrtpath).lethal_distance = 10.0;
			(*rrtpath).step = 40.0;
			fail_time = 1;
		}
		else {                     // ����·���滮 ��ز���
			expand = EXPAND;
			(*rrtpath).lethal_distance = 35;
			(*rrtpath).step = 15.0;
		}
		//udp_recv.Recv(1);
		get_map(udp_recv);   // ��õ�ͼ �뵱ǰ��������Ϣ
		int x = int(udp_recv.my_robot.x);
		int y = int(udp_recv.my_robot.y);
		//cout << x << " " << endl;
		pointnumber_smooth = 0;
		(*rrtpath).costs = map_cost;
		(*rrtpath).test_car_number = test_car_number;
		(*rrtpath).number_map_blue_obstacles = number_map_blue_obstacles;
		(*rrtpath).number_map_yellow_obstacles = number_map_yellow_obstacles;
		(*rrtpath).map_blue_obstacles_x = map_blue_obstacles_x;

		//			cout << "(*rrtpath).map_blue_obstacles_x[1] "<<(*rrtpath).map_blue_obstacles_x[1] << endl;
		(*rrtpath).map_blue_obstacles_y = map_blue_obstacles_y;
		//			cout << "(*rrtpath).map_blue_obstacles_x[1] " << (*rrtpath).map_blue_obstacles_x[1] << endl;
		(*rrtpath).map_yellow_obstacles_x = map_yellow_obstacles_x;
		(*rrtpath).map_yellow_obstacles_y = map_yellow_obstacles_y;

		//���յ���й滮
		if (flag == 0) {
			temp_x = goal_x;
			temp_y = goal_y;
		}    

		//�������й滮
		if (flag == 1) {
			temp_x = start_x;
			temp_y = start_y;
		}                     
		clock_t begintime, endtime;
		begintime = clock();

		// ʹ��RRT���й滮
		if ((*rrtpath).getPath(x, y, temp_x, temp_y)) {
			endtime = clock();
			cout << "path planning success!" << endl;
			cout << "plan time!!!!!!!!!!!!=" << double(endtime - begintime) / CLOCKS_PER_SEC << endl;

			pointnumber_smooth = (*rrtpath).get_pointnumber_smooth();
			tempx = (*rrtpath).get_x_path_smooth();
			tempy = (*rrtpath).get_y_path_smooth();

			for (int i = 0; i < pointnumber_smooth; i++) {
				x_path_smooth[i] = tempx[i];
				y_path_smooth[i] = tempy[i];
				cout << x_path_smooth[i] << " - " << y_path_smooth[i] << endl;
			}
			Get_new_path(x_path_smooth, y_path_smooth, pointnumber_smooth);
			for (int i = 0; i < new_point_num; i++) {
				cout << x_new_path[i] << " - - " << y_new_path[i] << endl;
			}
		}
		else
		{
			fail_time = (fail_time + 1) % 20;
			cout << "compute fail" << endl;
			continue;
		}

		
		/********************************************************************************/

		Debug_Msgs lines_set;
	
		begintime = clock();
		setpath(lines_set, x_path_smooth, y_path_smooth, pointnumber_smooth - 1);
		endtime = clock();

		//cout << "debug time!!!!!!!!!=" << double(endtime - begintime) / CLOCKS_PER_SEC << endl;

		/*********************************************************************************/

		int i = 0;
		double* vel;
		//cout << "=" << pointnumber_smooth << endl;

		// �ڶ���ѭ���������յ���������ϰ���ʱ ���¹滮
		while (i < new_point_num - 1)
		{
			int k = i / 3;   // ��¼·��ת�۵�

			// ʵʱ���µ�ͼ��Ϣ
			get_map(udp_recv);
			(*rrtpath).costs = map_cost;
			(*rrtpath).test_car_number = test_car_number;
			(*rrtpath).number_map_blue_obstacles = number_map_blue_obstacles;
			(*rrtpath).number_map_yellow_obstacles = number_map_yellow_obstacles;
			(*rrtpath).map_blue_obstacles_x = map_blue_obstacles_x;

			//			cout << "(*rrtpath).map_blue_obstacles_x[1] "<<(*rrtpath).map_blue_obstacles_x[1] << endl;
			(*rrtpath).map_blue_obstacles_y = map_blue_obstacles_y;
			//			cout << "(*rrtpath).map_blue_obstacles_x[1] " << (*rrtpath).map_blue_obstacles_x[1] << endl;
			(*rrtpath).map_yellow_obstacles_x = map_yellow_obstacles_x;
			(*rrtpath).map_yellow_obstacles_y = map_yellow_obstacles_y;
			(*rrtpath).lethal_distance = 20.0;

			// ��õ�ǰ�����˵����� �������
			double x_1, y_1;
			double orientation;

			x_1 = udp_recv.my_robot.x;
			y_1 = udp_recv.my_robot.y;
			orientation = udp_recv.my_robot.orientation;
			vel = setv(x_1, y_1, x_new_path[i + 1], y_new_path[i + 1], x_new_path[3 * k], y_new_path[3 * k], x_new_path[3 * k + 3], y_new_path[3 * k + 3], orientation, 0);
			udp_send.send(test_car_number, vel[0], vel[1], 0, false, 0, 0);

			udp_recv.Recv();
			x_1 = udp_recv.my_robot.x;
			y_1 = udp_recv.my_robot.y;

			// �жϳ�ǰ�����ϰ��� ����ѭ�������¹滮
			if (rrtpath->check_if_on_obstacle(x_1, y_1, x_new_path[i + 1], y_new_path[i + 1])) {
				udp_send.send(test_car_number, 0, 0, 0, false, 0, 0);
				cout << " ���ϰ������¹滮rrt�� " << endl;
				break;
			}

			// �����յ㸽�������� 
			if (i == new_point_num - 2) {
				if (Get_distance(x_1, y_1, start_x, start_y) <= offset && flag == 1)
				{
					flag = 0;
					cout << " ������㣡���¹滮��" << endl;
					break;
				}
				if (Get_distance(x_1, y_1, goal_x, goal_y) <= offset && flag == 0)
				{
					flag = 1;
					cout << " �����յ㣡���¹滮��" << endl;
					break;
				}
				orientation = udp_recv.my_robot.orientation;
				vel = setv(x_1, y_1, x_new_path[i + 1], y_new_path[i + 1], x_new_path[3 * k], y_new_path[3 * k], x_new_path[3 * k + 3], y_new_path[3 * k + 3], orientation, 1);
				udp_send.send(test_car_number, vel[0], vel[1], 0, false, 0, 0);
				continue;
			}

			//����Ŀ��ڵ㣬 ��������һ���ڵ�ǰ��
			if (sqrt((x_new_path[i + 1] - x_1)*(x_new_path[i + 1] - x_1) + (y_new_path[i + 1] - y_1)*(y_new_path[i + 1] - y_1)) <= 9) {
				i++;
				continue;
			}

		}
		
	}
	delete rrtpath;
	system("pause");
	return 0;
}

