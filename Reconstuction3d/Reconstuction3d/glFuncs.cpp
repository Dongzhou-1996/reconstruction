#include "header.h"
using namespace cv;


#if !defined(GLUT_WHEEL_UP)
#  define GLUT_WHEEL_UP   3
#  define GLUT_WHEEL_DOWN 4
#endif


//������������
void MapTexTri(Mat & texImg, Point2f pt2D[3], Point3d pt3D[3])
{
	// ������˺���
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);//��ͼƬ����������ӳ�䵽��Ļ����
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, texImg.cols, texImg.rows, 0,//����2D����
		GL_RGB, GL_UNSIGNED_BYTE, texImg.data);


	glBegin(GL_TRIANGLES);// ��ʼ������������

	glTexCoord2f(pt2D[0].x, pt2D[0].y); glVertex3f(pt3D[0].x, -pt3D[0].y, -pt3D[0].z);//��������
	glTexCoord2f(pt2D[1].x, pt2D[1].y); glVertex3f(pt3D[1].x, -pt3D[1].y, -pt3D[1].z);
	glTexCoord2f(pt2D[2].x, pt2D[2].y); glVertex3f(pt3D[2].x, -pt3D[2].y, -pt3D[2].z);

	glEnd();

}

// ������ά����

GLuint Create3DTexture(Mat &img, vector<Vec3i> &tri,
	vector<Point2f> pts2DTex, vector<Point3d> &structure, Point3d center3D, Vec3d size3D)
{
	GLuint tex = glGenLists(1);// ��ʾ�б��ʼ��
	int error = glGetError();
	if (error != GL_NO_ERROR)
		cout << "An OpenGL error has occured: " << gluErrorString(error) << endl;
	if (tex == 0) return 0;

	Mat texImg;
	cvtColor(img, img, CV_BGR2RGB);  //����ͼת��ΪRGBͼ��
	resize(img, texImg, Size(400, 300)); // ������ͼ�Ĵ�С

	glNewList(tex, GL_COMPILE);//����һ����ʾ�б�

	vector<Vec3i>::iterator iterTri = tri.begin();// ������
	Point2f pt2D[3];
	Point3d pt3D[3];

	glDisable(GL_BLEND); // �رջ��
	glEnable(GL_TEXTURE_2D); // ��������ӳ��
	for (; iterTri != tri.end(); iterTri++)//ѭ��tri
	{
		Vec3i &vertices = *iterTri;
		int ptIdx;
		for (int i = 0; i < 3; i++) // ѭ��������������
		{
			ptIdx = vertices[i];
			if (ptIdx == -1) break;
			pt2D[i].x = pts2DTex[ptIdx].x / img.cols;
			pt2D[i].y = pts2DTex[ptIdx].y / img.rows;
			pt3D[i] = (structure[ptIdx] - center3D) * (1.f / max(size3D[0], size3D[1]));//1.f ��c++�����ʾ��������1
		}

		if (ptIdx != -1)
		{
			MapTexTri(texImg, pt2D, pt3D);
		}
	}
	glDisable(GL_TEXTURE_2D);// �ر�����ӳ��

	glEndList();//�滻��ʾ�б�
	return tex;

}

#define PI_180			(CV_PI/180)
#define ROTATE_STEP		5
#define TRANSLATE_STEP	.3

static float	g_rx, g_ry;
static float	g_tz;
static GLuint	g_tex;

// ����ʾ���ڽ��г�ʼ��
void InitGl()
{
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(600, 600); // ��ʼ�����ڴ�С
	glutInitWindowPosition(100, 100); // ��ʼ������λ��

	glutCreateWindow("3D reconstruction");

	glClearColor(0, 0, 0, 1);
	glutDisplayFunc(displayGl); // չʾ
	glutReshapeFunc(resizeGl); // ��������
	glutKeyboardFunc(keyboard_control_Gl); // �ػ溯��
	glutSpecialFunc(special_control_Gl); // key��������ת
	glutMouseFunc(mouseGl); // �ػ溯��
	glutMotionFunc(mouse_move_Gl); // ���������ƷŴ���С

	wglUseFontBitmaps(wglGetCurrentDC(), 0, 256, 1000);
	glEnable(GL_DEPTH_TEST); // ����������Ȼ������Ĺ���
}

void Show(GLuint tex, Point3d center3D, Vec3d size3D)
{
	g_tz = 2; // �������λ��
	g_rx = 90;
	g_ry = 0;
	g_tex = tex;
	glutMainLoop(); // ����GLUT�¼�����ѭ��
}

void displayGl()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);// ���õ�ǰ����Ϊģ�;���
	glPushMatrix(); // ����ǰ���󱣴����ջ��
	// �������������ϵ�е�����
	//glRotatef(180.0, 0.0, 1.0, 0.0);
	float eyey = g_tz*sin(g_ry*PI_180),
		eyex = g_tz*cos(g_ry*PI_180)*cos(g_rx*PI_180),
		eyez = g_tz*cos(g_ry*PI_180)*sin(g_rx*PI_180);
	gluLookAt(eyex, eyey, eyez, 0, 0, 0, 0, 1, 0);// �����Ӿ��������������λ�úͷ������۲����壨��������������λ��-�����ͷ��׼�����������������λ��-������ϵķ��������������еķ���
	TRACE("%.1f,%.1f,%.1f,%.1f,%.1f\n", g_rx, g_ry, eyex, eyey, eyez);

	//glColor3f(1, 1, 1);
	glCallList(g_tex);// ִ����ʾ�б�

	glPopMatrix();// ��ǰ�����ջ
	glPushMatrix();
	//glColor3f(0, 1, 0);
	//glTranslatef(-0.08, 0.08, -0.2);//ƽ�ƺ���
	
	glListBase(1000);// ʹ��OpengL�����ҵ����ƶ�Ӧ�ַ�����ʾ�б��λ��
	glRasterPos3f(0, 0, 0);//ͼ��ƽ��
	string help = "use arrow keys to rotate, mouse wheel to zoom";
	glCallLists(help.size(), GL_UNSIGNED_BYTE, help.c_str());

	glPopMatrix(); // ��ǰ�����ջ
	glFlush(); // ǿ��ˢ�»��壬��֤��ͼ�����ִ��,�����Ǵ洢�ڻ������еȴ�������OpenGL����
	glutSwapBuffers(); // ʵ��˫���弼�������л�ͼ
}

void resizeGl(int w, int h)
{
	glViewport(0, 0, (GLsizei)w, (GLsizei)h); // ͼ�λ��ƴ��������λ�úʹ�С
	glMatrixMode(GL_PROJECTION); // ���õ�ǰ����ΪͶӰ����
	glLoadIdentity(); // ���õ�ǰ����Ϊ��λ����
	gluPerspective(45, GLdouble(w) / GLdouble(h), 0.01, 10000.0); // ָ���۲���Ӿ��������������еĴ�С���۾������ĽǶ�-ʵ�ʴ��ں��ݱ�-�����Ĳ���-Զ���Ĳ��棩
	glMatrixMode(GL_MODELVIEW); //  ���õ�ǰ����Ϊģ�;���
	glLoadIdentity(); // ���õ�ǰ����Ϊ��λ����
}

void mouseGl(int button, int state, int x, int y)
{
	switch (button)
	{
	case GLUT_WHEEL_UP: // �Ŵ�
		g_tz -= TRANSLATE_STEP;
		break;

	case  GLUT_WHEEL_DOWN: // ��С
		g_tz += TRANSLATE_STEP;
		break;

	default:
		break;
	}
	if (g_tz < 0) g_tz = 0;
	glutPostRedisplay();
}

void mouse_move_Gl(int x, int y)
{

	glutPostRedisplay();
}

void keyboard_control_Gl(unsigned char key, int a, int b)
{
	if (key == 0x1B)
		exit(1);
	glutPostRedisplay();//��ʾ��ǰ�������»���
}

void special_control_Gl(int key, int x, int y)
{
	if (key == GLUT_KEY_LEFT)
	{
		g_rx -= ROTATE_STEP;
		if (g_rx<1) g_rx = 1;
	}
	else if (key == GLUT_KEY_RIGHT)
	{
		g_rx += ROTATE_STEP;
		if (g_rx >= 179) g_rx = 179;
	}
	else if (key == GLUT_KEY_UP)
	{
		g_ry -= ROTATE_STEP;
		if (g_ry<-89) g_ry = -89;
	}
	else if (key == GLUT_KEY_DOWN)
	{
		g_ry += ROTATE_STEP;
		if (g_ry >= 89) g_ry = 89;
	}
	glutPostRedisplay();
}
