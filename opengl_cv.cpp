#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <GL/gl.h>
#include <GL/freeglut.h>

#include <iostream>
#include <queue>
#include <stdio.h>
#include <math.h>

/** Constants */
const int minFaceSize = 80; // in pixel. The smaller it is, the further away you can go
const float cvCamViewAngleXDeg = 48.55;
const float cvCamViewAngleYDeg = 40.37;
const float cx = 362.9; // not egal to W/2 = 320
const float cy = 295.84; // Not egal to H/2 = 240
const float f = 500; //804.71
const float eyesGap = 6.5; //cm

/** Global variables */
//-- capture opencv
cv::String face_cascade_name = "/home/alexis/Support/opencv-2.4.10/data/haarcascades/haarcascade_frontalface_alt.xml";
cv::CascadeClassifier face_cascade;
cv::VideoCapture *capture = NULL;
cv::Mat frame;

//-- display
bool bFullScreen = false;
bool bDisplayCam = true;
bool bDisplayDetection = true;
bool bDisplayMode = false;
float camRatio = 0.5;

//-- dimensions
int windowWidth;
int windowHeight;
int camWidth;
int camHeight;

//-- opengl camera
GLdouble glCamX;
GLdouble glCamY;
GLdouble glCamZ;

/** Functions */
void redisplay();
cv::Mat detectEyes(cv::Mat image);

void setGlCamera();
void draw3dScene();
void drawScreen();
void drawCube(float x, float y, float z, float l, float angle, float ax, float ay, float az );
void drawAxes(float length);

void onReshape( int w, int h );
void onMouse( int button, int state, int x, int y );
void onKeyboard( unsigned char key, int x, int y );
void onIdle();


/**
 * @function main
 */
int main( int argc, char **argv )
{
    // OPENCV INIT
    //-- Load the cascades
    if( !face_cascade.load( face_cascade_name ) )
    { printf("-- (!) ERROR loading 'haarcascade_frontalface_alt.xml'\nPlease edit face_cascade_name in source code.\n"); return -1; };

    // VIDEO CAPTURE
    //-- start video capture from camera
    capture = new cv::VideoCapture(0);

    //-- check that video is working
    if ( capture == NULL || !capture->isOpened() ) {
        fprintf( stderr, "Could not start video capture\n" );
        return 1;
    }

    // CAMERA IMAGE DIMENSIONS
    camWidth = (int) capture->get( CV_CAP_PROP_FRAME_WIDTH );
    camHeight = (int) capture->get( CV_CAP_PROP_FRAME_HEIGHT );

    // GLUT INIT
    glutInit( &argc, argv );
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    if(!bFullScreen){
        windowWidth = camWidth*1.5;
        windowHeight = camHeight*1.5;
        glutInitWindowPosition( 200, 80 );
        glutInitWindowSize( windowWidth, windowHeight );
    }
    glutCreateWindow( "Test OpenGL" );

    // RENDERING INIT
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING); //Enable lighting
    glEnable(GL_LIGHT0); //Enable light #0
    glEnable(GL_LIGHT1); //Enable light #1
    glEnable(GL_NORMALIZE); //Automatically normalize normals
    glShadeModel(GL_SMOOTH); //Enable smooth shading

    // SCREEN DIMENSIONS
    if(bFullScreen)
    {
        glutFullScreen();
        windowWidth = glutGet(GLUT_SCREEN_WIDTH);
        windowHeight = glutGet(GLUT_SCREEN_HEIGHT);
        glViewport( 0, 0, windowWidth, windowHeight );
    }

    // GUI CALLBACK FUNCTIONS
    glutDisplayFunc( redisplay );
    glutReshapeFunc( onReshape );
    glutMouseFunc( onMouse );
    glutKeyboardFunc( onKeyboard );
    glutIdleFunc( onIdle );

    // GUI LOOP
    glutMainLoop();

    return 0;
}

/**
 * @function display
 */
void redisplay()
{
    if(frame.empty()) {return;}
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // OPENCV
    //-- flip frame image
    cv::Mat tempimage;
    cv::flip(frame, tempimage, 0);
    //-- detect eyes
    tempimage = detectEyes(tempimage);

    // OPENGL
    //-- scene
    setGlCamera();
    draw3dScene();
    //-- cam
    if(bDisplayCam)
    {
        cv::flip(tempimage, tempimage, 0);
        cv::resize(tempimage, tempimage, cv::Size(camRatio*camWidth, camRatio*camHeight), 0, 0, cv::INTER_CUBIC);
        glDrawPixels( tempimage.size().width, tempimage.size().height, GL_BGR, GL_UNSIGNED_BYTE, tempimage.ptr() );
    }

    // RENDER
    //-- display on screen
    glutSwapBuffers();
    //-- post the next redisplay
    glutPostRedisplay();
}

cv::Mat detectEyes(cv::Mat image)
{
    // INIT
    std::vector<cv::Rect> faces;
    cv::Mat image_gray;
    cv::cvtColor( image, image_gray, CV_BGR2GRAY );
    cv::equalizeHist( image_gray, image_gray );

    // DETECT FACE
    //-- Find bigger face (opencv documentation)
    face_cascade.detectMultiScale( image_gray, faces, 1.1, 2, 0|CV_HAAR_SCALE_IMAGE|CV_HAAR_FIND_BIGGEST_OBJECT, cv::Size(minFaceSize, minFaceSize) );

    for( size_t i = 0; i < faces.size(); i++ )
    {
        // DETECT EYES
        //-- points in pixel
        cv::Point leftEyePt( faces[i].x + faces[i].width*0.30, faces[i].y + faces[i].height*0.37 );
        cv::Point rightEyePt( faces[i].x + faces[i].width*0.70, faces[i].y + faces[i].height*0.37 );
        cv::Point eyeCenterPt( faces[i].x + faces[i].width*0.5, leftEyePt.y );

        //-- normalize with webcam internal parameters
        GLdouble normRightEye = (rightEyePt.x - camWidth/2)/f;
        GLdouble normLeftEye = (leftEyePt.x - camWidth/2)/f;
        GLdouble normCenterX = (eyeCenterPt.x - camWidth/2)/f;
        GLdouble normCenterY = (eyeCenterPt.y - camHeight/2)/f;

        //-- get space coordinates
        glCamZ = eyesGap/(normRightEye-normLeftEye);
        glCamX = normCenterX*glCamZ;
        glCamY = -normCenterY*glCamZ;
        //std::cout<<"(x,y,z) = ("<<(int)glCamX<<","<<(int)glCamY<<","<<(int)glCamZ<<")"<<std::endl;

        // DISPLAY
        if(bDisplayCam && bDisplayDetection)
        {
            //-- face rectangle
            cv::rectangle(image, faces[i], 1234);

            //-- face lines
            cv::Point leftPt( faces[i].x, faces[i].y + faces[i].height*0.37 );
            cv::Point rightPt( faces[i].x + faces[i].width, faces[i].y + faces[i].height*0.37 );
            cv::Point topPt( faces[i].x + faces[i].width*0.5, faces[i].y);
            cv::Point bottomPt( faces[i].x + faces[i].width*0.5, faces[i].y + faces[i].height);
            cv::line(image, leftPt, rightPt, cv::Scalar( 0, 0, 0 ), 1, 1, 0);
            cv::line(image, topPt, bottomPt, cv::Scalar( 0, 0, 0 ), 1, 1, 0);

            //-- eyes circles
            cv::circle(image, rightEyePt, 0.06*faces[i].width, cv::Scalar( 255, 255, 255 ), 1, 8, 0);
            cv::circle(image, leftEyePt, 0.06*faces[i].width, cv::Scalar( 255, 255, 255 ), 1, 8, 0);

            //-- eyes line & center
            cv::line(image, leftEyePt, rightEyePt, cv::Scalar( 0, 0, 255 ), 1, 1, 0);
            cv::circle(image, eyeCenterPt, 2, cv::Scalar( 0, 0, 255 ), 3, 1, 0);
        }
    }
    return image;
}

void setGlCamera()
{
    // CAMERA PARAMETERS
    //-- set projection matrix using intrinsic camera params
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    //-- gluPerspective is arbitrarily set, you will have to determine these values based
    gluPerspective(60, (float)windowWidth/(float)windowHeight, 1, 250);
    //-- you will have to set modelview matrix using extrinsic camera params
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(glCamX, glCamY, glCamZ, 0, 0, 0, 0, 1, 0);
}

void draw3dScene()
{
    // DISPLAY MODE
    if(bDisplayMode){
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        drawAxes(10.0);
    }else{
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // LIGHTING
    //-- Add ambient light
    GLfloat ambientColor[] = {0.2f, 0.2f, 0.2f, 1.0f}; //Color (0.2, 0.2, 0.2)
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientColor);
    //-- Add positioned light
    GLfloat lightColor0[] = {0.5f, 0.5f, 0.5f, 1.0f}; //Color (0.5, 0.5, 0.5)
    GLfloat lightPos0[] = {4.0f, 0.0f, 8.0f, 1.0f}; //Positioned at (4, 0, 8)
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor0);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos0);
    //-- Add directed light
    GLfloat lightColor1[] = {0.5f, 0.2f, 0.2f, 1.0f}; //Color (0.5, 0.2, 0.2)
    //Coming from the direction (-1, 0.5, 0.5)
    GLfloat lightPos1[] = {-1.0f, 0.5f, 0.5f, 0.0f};
    glLightfv(GL_LIGHT1, GL_DIFFUSE, lightColor1);
    glLightfv(GL_LIGHT1, GL_POSITION, lightPos1);

    // GEOMETRY
    //-- Cube 1
    glColor3f(1.0f, 1.0f, 0.0f);
    drawCube(0.0, 0.0, 0.0, 6.0, 30.0, 0.0, 1.0, 0.0 );
    //-- Cube 2
    glColor3f(1.0f, 0.0f, 1.0f);
    drawCube(-20.0, 0.0, -40.0, 4, 70.0, 0.0, 1.0, 0.0 );
    //-- Cube 3
    glColor3f(0.0f, 1.0f, 1.0f);
    drawCube(5.0, 0.0, 10.0, 2.0, 10.0, 0.0, 1.0, 0.0 );

    // SCREEN BORDERS
    glColor3f(1.0f, 0.0f, 0.0f);
    drawScreen();
}

void drawScreen()
{
    float cx = (float)windowWidth/40.0;
    float cy = (float)windowHeight/40.0;

    glBegin(GL_LINE_LOOP);
        glVertex3f(cx, cy, 0);
        glVertex3f(cx, -cy, 0);
        glVertex3f(-cx, -cy, 0);
        glVertex3f(-cx, cy, 0);
    glEnd();
}

void drawCube(float x, float y, float z, float l, float angle, float ax, float ay, float az )
{
    glTranslatef(x, y, z);
    glRotatef(angle, ax, ay, az);

    glBegin(GL_QUADS);
        //Front
        glNormal3f(0.0, 0.0, l);
        //glNormal3f(-l, 0.0, l);
        glVertex3f(-l, -l, l);
        //glNormal3f(l, 0.0, l);
        glVertex3f(l, -l, l);
        //glNormal3f(l, 0.0, l);
        glVertex3f(l, l, l);
        //glNormal3f(-l, 0.0, l);
        glVertex3f(-l, l, l);

        //Right
        glNormal3f(l, 0.0, 0.0);
        //glNormal3f(l, 0.0, -l);
        glVertex3f(l, -l, -l);
        //glNormal3f(l, 0.0, -l);
        glVertex3f(l, l, -l);
        //glNormal3f(l, 0.0, l);
        glVertex3f(l, l, l);
        //glNormal3f(l, 0.0, l);
        glVertex3f(l, -l, l);

        //Back
        glNormal3f(0.0, 0.0, -l);
        //glNormal3f(-l, 0.0, -l);
        glVertex3f(-l, -l, -l);
        //glNormal3f(-l, 0.0, -l);
        glVertex3f(-l, l, -l);
        //glNormal3f(l, 0.0, -l);
        glVertex3f(l, l, -l);
        //glNormal3f(l, 0.0, -l);
        glVertex3f(l, -l, -l);

        //Left
        glNormal3f(-l, 0.0, 0.0);
        //glNormal3f(-l, 0.0, -l);
        glVertex3f(-l, -l, -l);
        //glNormal3f(-l, 0.0, l);
        glVertex3f(-l, -l, l);
        //glNormal3f(-l, 0.0, l);
        glVertex3f(-l, l, l);
        //glNormal3f(-l, 0.0, -l);
        glVertex3f(-l, l, -l);

    glEnd();
    glRotatef(-angle, ax, ay, az);
    glTranslatef(-x, -y, -z);
}

/**
 * @function drawAxes
 * A useful function for displaying your coordinate system
 */
void drawAxes(float length)
{

  glBegin(GL_LINES) ;
      glColor3f(1,0,0) ;
      glVertex3f(0,0,0) ;
      glVertex3f(length,0,0);

      glColor3f(0,1,0) ;
      glVertex3f(0,0,0) ;
      glVertex3f(0,length,0);

      glColor3f(0,0,1) ;
      glVertex3f(0,0,0) ;
      glVertex3f(0,0,length);
  glEnd() ;

}

/**
 * @function onReshape;
 */
void onReshape( int w, int h )
{
    windowWidth = w;
    windowHeight = h;
    glViewport( 0, 0, windowWidth, windowHeight );
}

/**
 * @function onMouse
 */
void onMouse( int button, int state, int x, int y )
{
  if ( button == GLUT_LEFT_BUTTON && state == GLUT_UP )
    {

    }
}

/**
 * @function onKeyboard
 */
void onKeyboard( unsigned char key, int x, int y )
{
    if( bFullScreen && ((key == 'f' || key == 'F') || ((int)key == 27)))
    {
        glutReshapeWindow(camWidth*1.5, camHeight*1.5);
        glutPositionWindow( 200, 80);
        bFullScreen = false;
    }
    else if(!bFullScreen && (key == 'f' || key == 'F'))
    {
        glutFullScreen();
        bFullScreen = true;
    }
    else switch ( key )
    {
        // change cam display
        case 'c': bDisplayCam = !bDisplayCam; break;
        case 'C': bDisplayCam = !bDisplayCam; break;

        // change detection display
        case 'd': bDisplayDetection = !bDisplayDetection; break;
        case 'D': bDisplayDetection = !bDisplayDetection; break;

        // change cam ratio
        case '+': if(camRatio < 1.9) camRatio += 0.1 ; break;
        case '-': if(camRatio > 0.2) camRatio -= 0.1; break;

        // change axes display
        case 'm': bDisplayMode = !bDisplayMode; break;
        case 'M': bDisplayMode = !bDisplayMode; break;

        // quit app
        case 'q': exit(0); break;
        case 'Q': exit(0); break;

        default: break;
    }
}

/**
 * @function onIdle
 */
void onIdle()
{
    (*capture) >> frame;
}

