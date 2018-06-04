#include "stdlib.h"  
#include <direct.h>  
#include <string.h>  
#include "header.h"

using namespace cv;


int main(int argc, char* argv[])
{
	//fountain
	/*Mat imgL = imread("E:\\Reconstuction3d\\Reconstuction3d\\fountain1.jpg"); 
	Mat	imgR = imread("fountain2.jpg");*/
	//church
	Mat imgL = imread("E:\\Reconstuction3d\\Reconstuction3d\\church1.jpg");
	Mat	imgR = imread("church2.jpg");
	//hand
	/*Mat imgL = imread("E:\\Reconstuction3d\\Reconstuction3d\\hand1.png");
	Mat	imgR = imread("hand2.png");*/
	//medusa
	/*Mat imgL = imread("E:\\Reconstuction3d\\Reconstuction3d\\medusa1.jpg");
	Mat	imgR = imread("medusa2.jpg");*/
	
	if (!(imgL.data) || !(imgR.data))//��
	{

		cerr<<"can't load image!"<<endl;
		exit(1);
	}

	/************************************************************************/
	/*������ͼ������Щ��Ӧ�ñ�ѡ�񣬲�����ͼ���м������Ӧ�� */
	/************************************************************************/
	cout<<"calculating feature points..."<<endl;
	//fountain����ڲ�
	/*Mat K(Matx33d(
		645.070482,0.000000,353.968508,
        0.000000,639.683979,234.946850,
        0.000000,0.000000,1.000000));*/
	//church����ڲ�
	Mat K(Matx33d(
		1663.782234,0.000000,785.889057,
        0.000000,1663.367425,638.790025,
        0.000000,0.000000,1.000000));
	//hand
	/*Mat K(Matx33d(
		667.168218,0.000000,321.060607,

		0.000000,670.287385,233.834066,

		0.000000,0.000000,1.000000));*/
	//medusa
	/*Mat K(Matx33d(
		1005.279068,0.000000,357.823593,

		0.000000,1102.528021,264.015057,

		0.000000,0.000000,1.000000));*/

	vector<vector<KeyPoint>> key_points_for_all;
	vector<Mat> descriptor_for_all;
	vector<vector<Vec3b>> colors_for_all;
	vector<DMatch> matches;
	//��ȡ��ƥ��������
	match_features(imgL, imgR, key_points_for_all, descriptor_for_all, colors_for_all, matches);
	
	vector<Point3d> structure;
	vector<vector<int>> correspond_struct_idx; //�����i��ͼ���е�j���������Ӧ��structure�е������
	vector<Vec3b> colors;
	vector<Mat> rotations;
	vector<Mat> motions;
	vector<Point2f>point1;
	vector<Point2f>point2;

	//��ʼ���ṹ����ά���ƣ�
	init_structure(
		imgL,
		imgR,
		K,
		key_points_for_all,
		colors_for_all,
		matches,
		structure,
		correspond_struct_idx,
		colors,
		rotations,
		motions,point1,point2
		);



	save_structure("E:\\Reconstuction3d\\Viewer1\\structure.yml", rotations, motions, structure, colors);
	//��BA���е���
	Mat intrinsic(Matx41d(K.at<double>(0, 0), K.at<double>(1, 1), K.at<double>(0, 2), K.at<double>(1, 2)));//����ڲζ���һ��4��1����
	vector<Mat> extrinsics;//��ξ���
	for (size_t i = 0; i < rotations.size(); ++i)//��ת�����ĸ���
	{
		Mat extrinsic(6, 1, CV_64FC1);//extrinsic����Ϊ6�������ľ���
		Mat r;
		Rodrigues(rotations[i], r);//����ת����ת��Ϊ��ת����

		r.copyTo(extrinsic.rowRange(0, 3));//��r��ǰ3�и��Ƶ�extrinsic
		motions[i].copyTo(extrinsic.rowRange(3, 6));//��motions[i]���Ƶ�extrinsic�ĺ�����

		extrinsics.push_back(extrinsic);
	}
	bundle_adjustment(intrinsic, extrinsics, correspond_struct_idx, key_points_for_all, structure);
    for (size_t i = 0; i < extrinsics.size(); ++i)
	{
		Mat extrinsic(6, 1, CV_64FC1);//extrinsic����Ϊ6�������ľ���
		Mat r;
		Rodrigues(rotations[i], r);//����ת����ת��Ϊ��ת����
		extrinsics[i].rowRange(0,3).copyTo(r);
		extrinsics[i].rowRange(3, 6).copyTo(motions[i]);
		Rodrigues(r, rotations[i]);//����ת����ת��Ϊ��ת����
	}
	save_structure("E:\\Reconstuction3d\\Viewer2\\structure.yml", rotations, motions, structure, colors);
	Point3d center3D;
	Vec3d size3D;
	StereoTo3D(point1,structure,imgL, center3D, size3D);

	/************************************************************************/
	/* �����ʷ�                                               */
	/************************************************************************/
	cout<<"doing triangulation..."<<endl;
	vector<Vec3i> tri;//����
	TriSubDiv(point1, imgL, tri);

	/************************************************************************/
	/*������ͼ                                          */
	/************************************************************************/
	glutInit(&argc, argv); // ����glut����ǰ,Ҫ��ʼ��glut
	InitGl(); // ����glut����ǰ,Ҫ��ʼ��glut

	cout<<"creating 3D texture..."<<endl;
	GLuint tex = Create3DTexture(imgL, tri, point1, structure, center3D, size3D);
	Show(tex,center3D, size3D);
	
	return 0;
}

