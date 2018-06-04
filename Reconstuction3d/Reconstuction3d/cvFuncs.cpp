#include <ceres\ceres.h>
#include <ceres\rotation.h>
#include <opencv2\tinydir\tinydir.h>

#include <fstream>
#include "header.h"
#include "legacy.h"

#define MAXM_FILTER_TH	.8	// threshold used in GetPair
#define HOMO_FILTER_TH	60	// threshold used in GetPair
#define NEAR_FILTER_TH	40	// diff points should have distance more than NEAR_FILTER_TH

using namespace cv;

//��ȡ��ƥ��������
void match_features(Mat &imgL, Mat &imgR, vector<vector<KeyPoint>>& key_points_for_all,
	vector<Mat>& descriptor_for_all, vector<vector<Vec3b>>& colors_for_all,vector<DMatch>& matches)
{   
	key_points_for_all.clear();
	descriptor_for_all.clear();
	vector<KeyPoint> keypointsL, keypointsR;
	Mat descriptorsL, descriptorsR;
	Ptr<Feature2D> sift = xfeatures2d::SIFT::create(0, 3, 0.04, 10);
	sift->detectAndCompute(imgL, noArray(), keypointsL, descriptorsL);
	sift->detectAndCompute(imgR, noArray(), keypointsR, descriptorsR);
	key_points_for_all.push_back(keypointsL);
	key_points_for_all.push_back(keypointsR);
	descriptor_for_all.push_back(descriptorsL);
	descriptor_for_all.push_back(descriptorsR);

	vector<Vec3b> colorsL(keypointsL.size());
	vector<Vec3b> colorsR(keypointsR.size());
	for (int i = 0; i < keypointsL.size(); ++i)
	{
		Point2f& p = keypointsL[i].pt;
		colorsL[i] = imgL.at<Vec3b>(p.y, p.x);
	}
	for (int i = 0; i < keypointsR.size(); ++i)
	{
		Point2f& p = keypointsR[i].pt;
		colorsR[i] = imgR.at<Vec3b>(p.y, p.x);
	}
	colors_for_all.push_back(colorsL);
	colors_for_all.push_back(colorsR);

	vector<vector<DMatch>> knn_matches;//DMatch Class for matching keypoint descriptors
	BFMatcher matcher(NORM_L2);//BFMatcherǿ��descriptorƥ������
	matcher.knnMatch(descriptor_for_all[0], descriptor_for_all[1], knn_matches, 2);//��ȡknn_matches[r][0]��knn_matches[r][1]
	
	matches.clear();
	for (size_t r = 0; r < knn_matches.size(); ++r)
	{
		matches.push_back(knn_matches[r][0]);
	}

	Mat OutImg1;
	drawMatches(imgL, key_points_for_all[0], imgR, key_points_for_all[1], matches,
	OutImg1, Scalar(255, 255, 255));
	/*namedWindow("KNN matching", WINDOW_NORMAL);
	imshow("KNN matching",OutImg1);*/
	char title1[100];
	sprintf_s(title1, 100, "KNN matching��%d", matches.size());
	namedWindow(title1, WINDOW_NORMAL);
	imshow(title1, OutImg1);
	
	
	//��ȡ����Ratio Test������ƥ������Сƥ��ľ���min_dist
	float min_dist = FLT_MAX;
	for (int r = 0; r < knn_matches.size(); ++r)
	{
		//���ʲ���
		if (knn_matches[r][0].distance > 0.6*knn_matches[r][1].distance)
			continue;

		float dist = knn_matches[r][0].distance;
		if (dist < min_dist) min_dist = dist;
	}

	matches.clear();
	for (size_t r = 0; r < knn_matches.size(); ++r)
	{
		//�ų���������ʲ��Եĵ��ƥ��������ĵ�
		if (
			knn_matches[r][0].distance > 0.6*knn_matches[r][1].distance ||
			knn_matches[r][0].distance > 5 * max(min_dist, 10.0f)
			)
			continue;

		//����ƥ���
		matches.push_back(knn_matches[r][0]);
	}
	Mat OutImg2;
	drawMatches(imgL, key_points_for_all[0], imgR, key_points_for_all[1], matches,
		OutImg2, Scalar(255, 255, 255));
	//namedWindow("Radio Test matching", WINDOW_NORMAL);
	//imshow("Radio Test matching", OutImg2);
	char title2[100];
	sprintf_s(title2, 100, "Radio Test matching��%d", matches.size());
	namedWindow(title2, WINDOW_NORMAL);
	imshow(title2, OutImg2);
}
//��ƥ����ӱ��������������
void get_matched_points(
	vector<KeyPoint>& p1,
	vector<KeyPoint>& p2,
	vector<DMatch> matches,
	vector<Point2f>& out_p1,
	vector<Point2f>& out_p2
	)
{
	out_p1.clear();
	out_p2.clear();
	for (int i = 0; i < matches.size(); ++i)
	{
		out_p1.push_back(p1[matches[i].queryIdx].pt);
		out_p2.push_back(p2[matches[i].trainIdx].pt);
	}
}
//��ƥ����ӱ����������RGB
void get_matched_colors(
	vector<Vec3b>& c1,
	vector<Vec3b>& c2,
	vector<DMatch> matches,
	vector<Vec3b>& out_c1,
	vector<Vec3b>& out_c2
	)
{
	out_c1.clear();
	out_c2.clear();
	for (int i = 0; i < matches.size(); ++i)
	{
		out_c1.push_back(c1[matches[i].queryIdx]);
		out_c2.push_back(c2[matches[i].trainIdx]);
	}
}
//��ȡ R  T
bool find_transform(Mat& K, vector<Point2f>& p1, vector<Point2f>& p2, Mat& R, Mat& T, Mat& mask)
{
	//�����ڲξ����ȡ����Ľ���͹������꣨�������꣩
	double focal_length = 0.5*(K.at<double>(0) + K.at<double>(4));
	Point2f principle_point(K.at<double>(2), K.at<double>(5));

	//����ƥ���ʹ��RANSAC��ȡ�������󣬽�һ���ų�ʧ���
	/*p1:����1������
	p2������2������
	focal_length������
	principle_point���������
	RANSAC
	0.999��RANSAC�Ĳ����� ���Ǵӵ㵽�Լ���������������Ϊ��λ�������õ㱻��Ϊ��ʧ��㣬�����ڼ��㣬���ջ�������
	1.0������RANSAC��LMedS�����Ĳ����� ��ָ����һ�������ˮƽ���Ŷȣ����ʣ����ƾ�������ȷ��
	mask���������ÿ��Ԫ�ص�����Ϊʧ�������Ϊ0��Ϊ1Ϊ�����㡣������ͼ1��ͼ2��
	*/

	Mat E = findEssentialMat(p1, p2, focal_length, principle_point, RANSAC, 0.999, 1.0, mask);//���ӽ����������������������p1p2������������㣩
	if (E.empty()) return false;

	double feasible_count = countNonZero(mask);//����mask��ķ���Ԫ�����������˺�ƥ�����������Ŀ��
	cout << (int)feasible_count << " -in- " << p1.size() << endl;//�ɹ�ƥ��ĵ��p1������
	//����RANSAC���ԣ�outlier��������50%ʱ������ǲ��ɿ���
	if (feasible_count <= 15 || (feasible_count / p1.size()) < 0.6)
		return false;

	//�ֽⱾ�����󣬻�ȡ��Ա任
	int pass_count = recoverPose(E, p1, p2, R, T, focal_length, principle_point, mask);

	//ͬʱλ���������ǰ���ĵ������Ҫ�㹻��

	if (((double)pass_count) / feasible_count < 0.7)//pass_count�ɹ�ƥ��ĵ�
		return false;
	return true;
}
//��ȡRNASAC�ų�ʧ�����p1������
void maskout_points(vector<Point2f>& p1, Mat& mask)
{
	vector<Point2f> p1_copy = p1;
	p1.clear();

	for (int i = 0; i < mask.rows; ++i)
	{
		if (mask.at<uchar>(i) > 0)
			p1.push_back(p1_copy[i]);
	}
}
//��ȡRNASAC�ų�ʧ�����p1���������ɫ
void maskout_colors(vector<Vec3b>& p1, Mat& mask)
{
	vector<Vec3b> p1_copy = p1;
	p1.clear();

	for (int i = 0; i < mask.rows; ++i)
	{
		if (mask.at<uchar>(i) > 0)
			p1.push_back(p1_copy[i]);
	}
}
void reconstruct(Mat& K, Mat& R1, Mat& T1, Mat& R2, Mat& T2, vector<Point2f>& p1, vector<Point2f>& p2, vector<Point3d>& structure)
{
	//���������ͶӰ����[R T]��triangulatePointsֻ֧��float��
	Mat proj1(3, 4, CV_32FC1);
	Mat proj2(3, 4, CV_32FC1);

	R1.convertTo(proj1(Range(0, 3), Range(0, 3)), CV_32FC1);
	T1.convertTo(proj1.col(3), CV_32FC1);

	R2.convertTo(proj2(Range(0, 3), Range(0, 3)), CV_32FC1);
	T2.convertTo(proj2.col(3), CV_32FC1);

	Mat fK;//���н����ת������
	K.convertTo(fK, CV_32FC1);//Kת��ΪCV_32FC1���͵ľ���fK
	proj1 = fK*proj1;
	proj2 = fK*proj2;

	//�����ؽ�
	/*
	����������ͨ��ʹ������������۲��ؽ���ά��(���������)
	o projMatr1�C 3x4 ��һ�������ͶӰ����.3��4
	o projMatr2�C 3x4 �ڶ��������ͶӰ����.3��4
	o projPoints1�C 2xN ��һ��ͼ������������.
	o projPoints2�C 2xN�ڶ���ͼ������������.
	o points4D�C 4xN ���������ϵ֮���ع�������,������������Ŀ����Ҫ�Ǻϲ����������еĳ˷��ͼӷ�(��ά��任�������ǰ)
	*/

	Mat s;//structure
	triangulatePoints(proj1, proj2, p1, p2, s);

	structure.clear();
	structure.reserve(s.cols);//����structure������С
	for (int i = 0; i < s.cols; ++i)//4��N��ÿһ����һ������
	{
		Mat_<float> col = s.col(i);
		col /= col(3);	//������꣬��Ҫ�������һ��Ԫ�ز�������������ֵ
		structure.push_back(Point3f(col(0), col(1), col(2)));//����ά�ؽ������structure��
	}
}

void save_structure(string file_name, vector<Mat>& rotations, vector<Mat>& motions, vector<Point3d>& structure, vector<Vec3b>& colors)
{
	int n = (int)rotations.size();

	FileStorage fs(file_name, FileStorage::WRITE);
	fs << "Camera Count" << n;
	fs << "Point Count" << (int)structure.size();

	fs << "Rotations" << "[";
	for (size_t i = 0; i < n; ++i)
	{
		fs << rotations[i];
	}
	fs << "]";

	fs << "Motions" << "[";
	for (size_t i = 0; i < n; ++i)
	{
		fs << motions[i];
	}
	fs << "]";

	fs << "Points" << "[";
	for (size_t i = 0; i < structure.size(); ++i)
	{
		fs << structure[i];//�����ά��
	}
	fs << "]";

	fs << "Colors" << "[";
	for (size_t i = 0; i < colors.size(); ++i)
	{
		fs << colors[i];
	}
	fs << "]";

	fs.release();
}
//�����ǳ�ʼ�����ƣ�Ҳ����ͨ��˫Ŀ�ؽ�������ͼ�����е�ͷ����ͼ������ؽ�������ʼ��correspond_struct_idx��
void init_structure(
	Mat &imgL,
	Mat &imgR,
	Mat K,
	vector<vector<KeyPoint>>& key_points_for_all,
	vector<vector<Vec3b>>& colors_for_all,
	vector<DMatch>& matches,
	vector<Point3d>& structure,
	vector<vector<int>>& correspond_struct_idx,
	vector<Vec3b>& colors,
	vector<Mat>& rotations,
	vector<Mat>& motions,
	vector<Point2f>&point1,
	vector<Point2f>&point2
	)
{
	//����ͷ����ͼ��֮��ı任����
	vector<Point2f> p1, p2;
	vector<Vec3b> c2;
	Mat R, T;	//��ת�����ƽ������
	Mat mask;	//mask�д�����ĵ����ƥ��㣬���������ʧ���
	get_matched_points(key_points_for_all[0], key_points_for_all[1], matches, p1, p2);//��ƥ����ӱ��������������
	get_matched_colors(colors_for_all[0], colors_for_all[1], matches, colors, c2);//��ƥ����ӱ������������ɫ
	find_transform(K, p1, p2, R, T, mask);//��ȡR T

	Mat OutImg3;
	drawMatches(imgL, key_points_for_all[0], imgR, key_points_for_all[1], matches,
		OutImg3, Scalar(255, 255, 255));
	/*namedWindow("RANSAC matching", WINDOW_NORMAL);
	imshow("RANSAC matching", OutImg3);*/
	char title[100];
	sprintf_s(title, 100, "RANSAC matching��%d", matches.size());
	namedWindow(title, WINDOW_NORMAL);
	imshow(title, OutImg3);


	//��ͷ����ͼ�������ά�ؽ�
	maskout_points(p1, mask);//��ȡRNASAC�ų�ʧ�����p1������
	maskout_points(p2, mask);//��ȡRNASAC�ų�ʧ�����p2������
	maskout_colors(colors, mask);//��ȡRNASAC�ų�ʧ�����p1���������ɫ
	Mat R0 = Mat::eye(3, 3, CV_64FC1);//���õ�һ��ͼ���R0T0
	Mat T0 = Mat::zeros(3, 1, CV_64FC1);

	reconstruct(K, R0, T0, R, T, p1, p2, structure);//������ά�ؽ�
	point1 = p1;
	point2 = p2;

	//����任����
	rotations = { R0, R };
	motions = { T0, T };

	//��correspond_struct_idx�Ĵ�С��ʼ��Ϊ��key_points_for_all��ȫһ��
	correspond_struct_idx.clear();
	correspond_struct_idx.resize(key_points_for_all.size());//ͼƬ����ͬ
	for (int i = 0; i < key_points_for_all.size(); ++i)
	{
		correspond_struct_idx[i].resize(key_points_for_all[i].size(), -1);//ÿ��ͼ�������������Ϊ��ͬ��
	}

	//��дͷ����ͼ��Ľṹ����
	int idx = 0;
	for (int i = 0; i < matches.size(); ++i)
	{
		if (mask.at<uchar>(i) == 0)
			continue;
		//idx��Ϊ����ƥ��������������
		correspond_struct_idx[0][matches[i].queryIdx] = idx;//idx��Ϊ��0��ͼ���i��ƥ��������
		correspond_struct_idx[1][matches[i].trainIdx] = idx;//idx��Ϊ��1��ͼ���i��ƥ��������
		++idx;
	}
}
//���ۣ�coss������
struct ReprojectCost
{
	cv::Point2d observation;//�����������

	ReprojectCost(cv::Point2d& observation)
		: observation(observation)
	{
	}

	template <typename T>
	bool operator()(const T* const intrinsic, const T* const extrinsic, const T* const pos3d, T* residuals) const//residuals�в���ָʵ�ʹ۲�ֵ�����ֵ�����ֵ��֮��Ĳ�
	{
		const T* r = extrinsic;
		const T* t = &extrinsic[3];

		T pos_proj[3];
		ceres::AngleAxisRotatePoint(r, pos3d, pos_proj);//����������ϵ����ά�������ת�任

		//ƽ�Ʊ任��ת��Ϊ���������ϵ
		pos_proj[0] += t[0];
		pos_proj[1] += t[1];
		pos_proj[2] += t[2];

		const T x = pos_proj[0] / pos_proj[2];
		const T y = pos_proj[1] / pos_proj[2];

		const T fx = intrinsic[0];//intrinsic�� 4��1���ڲξ���
		const T fy = intrinsic[1];
		const T cx = intrinsic[2];
		const T cy = intrinsic[3];


		const T u = fx * x + cx;//u,v��ͼƬ����ϵ���ص��к���
		const T v = fy * y + cy;

		residuals[0] = u - T(observation.x);//x������ͶӰ������ͶӰֵ-ʵ�ʹ۲�ֵ��
		residuals[1] = v - T(observation.y);//y������ͶӰ���

		return true;
	}
};
//Ceres Solver���BA������ʹ����Ceres�ṩ��Huber������Ϊ��ʧ��loss������
void bundle_adjustment(
	Mat& intrinsic,        //�ڲξ���
	vector<Mat>& extrinsics, //��ξ���
	vector<vector<int>>& correspond_struct_idx,//correspond_struct_idx[i][j]�����i��ͼ���j������������Ӧ�Ŀռ���ڵ����е�����
	vector<vector<KeyPoint>>& key_points_for_all, //������
	vector<Point3d>& structure  //��ά�㼯
	)
{
	ceres::Problem problem;

	// ������Σ�R T��
	for (size_t i = 0; i < extrinsics.size(); ++i)
	{
		problem.AddParameterBlock(extrinsics[i].ptr<double>(), 6);
	}
	// ���Ż��ڼ䱣��ָ���Ĳ����飨��һ��������ڲΣ�����
	problem.SetParameterBlockConstant(extrinsics[0].ptr<double>());

	// �����ڲ�
	problem.AddParameterBlock(intrinsic.ptr<double>(), 4); // fx, fy, cx, cy

	// ����������
	ceres::LossFunction* loss_function = new ceres::HuberLoss(4);   // loss function make bundle adjustment robuster.
	for (size_t img_idx = 0; img_idx < correspond_struct_idx.size(); ++img_idx)//img_idx��ʾ�ڼ���ͼƬ
	{
		vector<int>& point3d_ids = correspond_struct_idx[img_idx];//��img_idx��ͼƬ����ά��������
		vector<KeyPoint>& key_points = key_points_for_all[img_idx];//��img_idx��ͼƬ�������㼯
		for (size_t point_idx = 0; point_idx < point3d_ids.size(); ++point_idx)//point_idx��ʾ�ڼ���3ά���������
		{
			int point3d_id = point3d_ids[point_idx];//ָ����ά��ı�ǩ
			if (point3d_id < 0)
				continue;

			Point2d observed = key_points[point_idx].pt;//�����������
			// ģ������У���һ��Ϊ���ۺ��������ͣ��ڶ���Ϊ���۵�ά�ȣ�ʣ�������ֱ�Ϊ���ۺ�����һ�ڶ����е�����������ά��
			ceres::CostFunction* cost_function = new ceres::AutoDiffCostFunction<ReprojectCost, 2, 4, 6, 3>(new ReprojectCost(observed));

			//���زв��йصĲ���
			problem.AddResidualBlock(
				cost_function,
				loss_function,
				intrinsic.ptr<double>(),            // Intrinsic
				extrinsics[img_idx].ptr<double>(),  // View Rotation and Translation
				&(structure[point3d_id].x)          // Point in 3D space
				);
		}
	}

	// Solve BA
	ceres::Solver::Options ceres_config_options;//ѡ��ṹ����������ε�ѡ��
	ceres_config_options.minimizer_progress_to_stdout = false;//Ĭ������£�Minimizer���ȼ�¼��VLOG
	ceres_config_options.logging_type = ceres::SILENT;//logging����
	ceres_config_options.num_threads = 1;
	ceres_config_options.preconditioner_type = ceres::JACOBI;//��������������һ��ʹ�õ�Ԥ����������
	ceres_config_options.linear_solver_type = ceres::ITERATIVE_SCHUR;//��С���˷�ѡ��
	ceres_config_options.sparse_linear_algebra_library_type = ceres::EIGEN_SPARSE;//ϡ�����Դ���������

	ceres::Solver::Summary summary;
	ceres::Solve(ceres_config_options, &problem, &summary);//���ϡ���������Է���

	if (!summary.IsSolutionUsable())//summary�Ƿ����
	{
		std::cout << "Bundle Adjustment failed." << std::endl;
	}
	else
	{
		// Display statistics about the minimization
		std::cout << std::endl
			<< "Bundle Adjustment statistics (approximated RMSE):\n"
			<< " #views: " << extrinsics.size() << "\n"//ͼƬ����
			<< " #residuals: " << summary.num_residuals << "\n"//�в������
			<< " Initial RMSE: " << std::sqrt(summary.initial_cost / summary.num_residuals) << "\n"//�Ż�ǰ��ƽ������ͶӰ��Initial RMSE��
			<< " Final RMSE: " << std::sqrt(summary.final_cost / summary.num_residuals) << "\n"//�Ż���ĸ�ֵ��Final RMSE��
			<< " Time (s): " << summary.total_time_in_seconds << "\n"//��Solve������ʱ���ܹ�������Ceres�е�����ʱ��
			<< std::endl;
	}
}
//ʹ��opencv�Ĺ��ܽ��������ʷ�
bool isGoodTri( Vec3i &v, vector<Vec3i> & tri ) //Vec3i�д洢����3����ʾ�����ŵ�����,tri�洢���е�������
{
	int a = v[0], b = v[1], c = v[2];
	v[0] = min(a,min(b,c));//�����������Сֵ
	v[2] = max(a,max(b,c));//������������ֵ
	v[1] = a+b+c-v[0]-v[2];
	if (v[0] == -1) return false;
	//����tri������������
	vector<Vec3i>::iterator iter = tri.begin();//������(Iterator) ��������һ�����ģʽ,����һ������,�����Ա�����ѡ�������еĶ���
	for(;iter!=tri.end();iter++)
	{
		Vec3i &check = *iter;
		if (check[0]==v[0] &&//��
			check[1]==v[1] &&
			check[2]==v[2])
		{
			break;
		}
	}
	if (iter == tri.end())
	{
		tri.push_back(v);
		return true;
	}
	return false;
}
//�����뷽ʽ���ɵ�Delaunay���������㷨��Ҫ����Bowyer-Watson�㷨

void TriSubDiv( vector<Point2f> &pts, Mat &img, vector<Vec3i> &tri )//��ͼ������-��ͼ-tri 
{
	CvSubdiv2D* subdiv;//The subdivision itself // ϸ�� 
	CvMemStorage* storage = cvCreateMemStorage(0);//�����洢�����ʷ� 
	Rect rc = Rect(0,0, img.cols, img.rows); //���ǵ���ӱ߽���� 

	subdiv = cvCreateSubdiv2D( CV_SEQ_KIND_SUBDIV2D, sizeof(*subdiv),//�����µ�ϸ��
		sizeof(CvSubdiv2DPoint),
		sizeof(CvQuadEdge2D),
		storage );//Ϊ��������ռ�  

	cvInitSubdivDelaunay2D( subdiv, rc );//��ʼ��Delaunay�����ʷ� 

	//������ǵĵ㼯����32λ�ģ����������ǽ���תΪCvPoint2D32f���������ַ�����
	for (size_t i = 0; i < pts.size(); i++)
	{
		CvSubdiv2DPoint *pt = cvSubdivDelaunay2DInsert( subdiv, pts[i] );//����һ���µĵ㵽Delaunay�����ʷ�
		pt->id = i;
	}

	CvSeqReader reader;
	int total = subdiv->edges->total;//Ԫ������
	int elem_size = subdiv->edges->elem_size;//����Ԫ�صĴ�С�����ֽڼƣ�
	//��ʼ�������Ķ��������п�����ǰ������ȡ
	cvStartReadSeq( (CvSeq*)(subdiv->edges), &reader, 0 );
	Point buf[3];//�����������
	const Point *pBuf = buf;//ָ�����������ָ��
	Vec3i verticesIdx;//��������
	Mat imgShow = img.clone();

	srand( (unsigned)time( NULL ) );   
	for( int i = 0; i < total; i++ )//ѭ�����еı�
	{   
		CvQuadEdge2D* edge = (CvQuadEdge2D*)(reader.ptr);   
		//���ptrָ���Ԫ���Ƿ�����һ������
		if( CV_IS_SET_ELEM( edge ))//ѭ��һ�������ε���������
		{
			CvSubdiv2DEdge t = (CvSubdiv2DEdge)edge; 
			int iPointNum = 3;
			Scalar color = CV_RGB(rand()&255,rand()&255,rand()&255);//���ȡɫ
			int j;
			for(j = 0; j < iPointNum; j++ )//ѭ����������
			{
				CvSubdiv2DPoint* pt = cvSubdiv2DEdgeOrg( t );
				if( !pt ) break;
				buf[j] = pt->pt;
				verticesIdx[j] = pt->id;//��������
				t = cvSubdiv2DGetEdge( t, CV_NEXT_AROUND_LEFT );//�������������һ����
			}
			if (j != iPointNum) continue;
			if (isGoodTri(verticesIdx, tri))//�Ƿ���������ʷ�
			{   //����������
				polylines( imgShow, &pBuf, &iPointNum, 
					1, true, color,
					1, CV_AA, 0);
			}
			//���������
			t = (CvSubdiv2DEdge)edge+2;

			for(j = 0; j < iPointNum; j++ )
			{
				CvSubdiv2DPoint* pt = cvSubdiv2DEdgeOrg( t );
				if( !pt ) break;
				buf[j] = pt->pt;
				verticesIdx[j] = pt->id;
				t = cvSubdiv2DGetEdge( t, CV_NEXT_AROUND_LEFT );
			}   
			if (j != iPointNum) continue;
			if (isGoodTri(verticesIdx, tri))
			{
				//����������
				polylines( imgShow, &pBuf, &iPointNum, 
					1, true, color,
					1, CV_AA, 0);
			}
		}

		CV_NEXT_SEQ_ELEM( elem_size, reader );

	}
    
	char title[100];
	sprintf_s(title, 100, "Delaunay: %d Triangles", tri.size());
	namedWindow(title, WINDOW_NORMAL);
	imshow(title, imgShow);
	waitKey();
}

void StereoTo3D(vector<Point2f> ptsL, vector<Point3d> structure, Mat img,//��ͼ
				Point3d &center3D, Vec3d &size3D) //�����������������ʹ�С
{
	double minX = 1e9, maxX = -1e9;//��Сֵ���ᳬ��1e9,���ֵ����С��-1e9������Լ����-1e9��1e9֮��
	double minY = 1e9, maxY = -1e9;//10�ľŴη���-10�ľŴη�
    double minZ = 1e9, maxZ = -1e9;

	for (int i = 0; i < structure.size(); ++i)
	{

		minX = min(minX, structure[i].x); maxX = max(maxX, structure[i].x);//ȡ���ֵ����Сֵ
		minY = min(minY, structure[i].y); maxY = max(maxY, structure[i].y);
		minZ = min(minZ, structure[i].z); maxZ = max(maxZ, structure[i].z);
	}
	//���Ƶ����ĵ������
	center3D.x = (minX+maxX)/2;//��������
	center3D.y = (minY+maxY)/2;
	center3D.z = (minZ+maxZ)/2;
	//����Բ�Ĵ�С
	size3D[0] = maxX-minX;//���ƿ��С
	size3D[1] = maxY-minY;
	size3D[2] = maxZ-minZ;
}
