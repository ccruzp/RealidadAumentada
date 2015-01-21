#ifdef _WIN32
#  include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef __APPLE__
#  include <GL/glut.h>
#else
#  include <GLUT/glut.h>
#endif
#include <AR/gsub.h>
#include <AR/param.h>
#include <AR/ar.h>
#include <AR/video.h>

#include "object.h"


#define COLLIDE_DIST 30000.0

/* Object Data */
char            *model_name = "Data/object_data3";
ObjectData_T    *object;
int             objectnum;

int             xsize, ysize;
int				thresh = 100;
int             count = 0;

/* set up the video format globals */

#ifdef _WIN32
char			*vconf = "Data\\WDM_camera_flipV.xml";
#else
char			*vconf = "";
#endif

char           *cparam_name    = "Data/camera_para.dat";
ARParam         cparam;

static void   init(void);
static void   cleanup(void);
static void   keyEvent( unsigned char key, int x, int y);
static void   mainLoop(void);
static int checkCollisions( ObjectData_T object0, ObjectData_T object1, float collide_dist);
static int draw( ObjectData_T *object, int objectnum );
static int  draw_object( int obj_id, double gl_para[16], int collide_flag );

int main(int argc, char **argv)
{

  printf("Startup\n");
  //initialize applications
  glutInit(&argc, argv);
  printf("Glut init finished\n");
  init();
  printf("Init finished\n");
  
  //start video capture
  printf("Starting video capture\n");
  arVideoCapStart();
  printf("Ended video capture\n");
  
  //start the main event loop
  argMainLoop( NULL, keyEvent, mainLoop );
  
  return 0;
}

static void   keyEvent( unsigned char key, int x, int y)   
{
  /* quit if the ESC key is pressed */
  if( key == 0x1b ) {
    printf("*** %f (frame/sec)\n", (double)count/arUtilTimer());
    cleanup();
    exit(0);
  }
}

/* main loop */
static void mainLoop(void)
{
  ARUint8         *dataPtr;
  ARMarkerInfo    *marker_info;
  int             marker_num;
  int             i,j,k;
  
  /* grab a video frame */
  if( (dataPtr = (ARUint8 *)arVideoGetImage()) == NULL ) {
    arUtilSleep(2);
    return;
  }
  
  if( count == 0 ) arUtilTimerReset();  
  count++;
  
  /*draw the video*/
  argDrawMode2D();
  argDispImage( dataPtr, 0,0 );
  
  /* capture the next video frame */
  arVideoCapNext();
  
  glColor3f( 1.0, 0.0, 0.0 );
  glLineWidth(6.0);
  
  /* detect the markers in the video frame */
  if(arDetectMarker(dataPtr, thresh, 
		    &marker_info, &marker_num) < 0 ) {
    cleanup(); 
    exit(0);
  }
  for( i = 0; i < marker_num; i++ ) {
    argDrawSquare(marker_info[i].vertex,0,0);
  }
  
  /* check for known patterns */
  for( i = 0; i < objectnum; i++ ) {
    k = -1;
    for( j = 0; j < marker_num; j++ ) {
      if( object[i].id == marker_info[j].id) {
	
	/* you've found a pattern */
	//printf("Found pattern: %d ",patt_id);
	glColor3f( 0.0, 1.0, 0.0 );
	argDrawSquare(marker_info[j].vertex,0,0);
	
	if( k == -1 ) k = j;
	else /* make sure you have the best pattern (highest confidence factor) */
	  if( marker_info[k].cf < marker_info[j].cf ) k = j;
      }
    }
    if( k == -1 ) {
      object[i].visible = 0;
      continue;
    }
    
    /* calculate the transform for each marker */
    if( object[i].visible == 0 ) {
      arGetTransMat(&marker_info[k],
		    object[i].marker_center, object[i].marker_width,
		    object[i].trans);
    }
    else {
      arGetTransMatCont(&marker_info[k], object[i].trans,
			object[i].marker_center, object[i].marker_width,
			object[i].trans);
    }
    object[i].visible = 1;
  }
  
  /*check for object collisions between markers */
  object[0].collide = 0;
  object[1].collide = 0;
  object[2].collide = 0;
  object[3].collide = 0;
  object[4].collide = 0;
  
  if(object[0].visible){
    if(object[3].visible && checkCollisions(object[0],object[3],COLLIDE_DIST)){
      object[0].collide = 1;
      object[3].collide = 1;

    }
    if(object[4].visible && checkCollisions(object[0],object[4],COLLIDE_DIST)){
      /* if (object[0].collide == 1) { */
      /* 	object[0].collide = 4; */
      /* } else { */
	object[0].collide = 2;
	object[4].collide = 2;
      /* } */

    } else if(object[5].visible && checkCollisions(object[0],object[5],COLLIDE_DIST)){
      object[0].collide = 3;
      object[5].collide = 3;
    }

  } else if (object[1].visible) {
    if(object[3].visible && checkCollisions(object[1],object[3],COLLIDE_DIST)){
      object[1].collide = 1;
      object[3].collide = 1;

    } else if(object[4].visible && checkCollisions(object[1],object[4],COLLIDE_DIST)){
      object[1].collide = 2;
      object[4].collide = 2;
    } else if(object[5].visible && checkCollisions(object[1],object[5],COLLIDE_DIST)){
      object[1].collide = 3;
      object[5].collide = 3;
    }
  } else if (object[2].visible) {
    if(object[3].visible && checkCollisions(object[2],object[3],COLLIDE_DIST)){
      object[2].collide = 1;
      object[3].collide = 1;
    } else if(object[4].visible && checkCollisions(object[2],object[4],COLLIDE_DIST)){
      object[2].collide = 2;
      object[4].collide = 2;
    } else if(object[5].visible && checkCollisions(object[2],object[5],COLLIDE_DIST)){
      object[2].collide = 3;
      object[5].collide = 3;
    }
  }    
   
  
  /* draw the AR graphics */
  draw( object, objectnum );

  /*swap the graphics buffers*/
  argSwapBuffers();
}

/* check collision between two markers */
static int checkCollisions( ObjectData_T object0, ObjectData_T object1, float collide_dist)
{
  float x1,y1,z1;
  float x2,y2,z2;
  float dist;
  x1 = object0.trans[0][3];
  y1 = object0.trans[1][3];
  z1 = object0.trans[2][3];
  
  x2 = object1.trans[0][3];
  y2 = object1.trans[1][3];
  z2 = object1.trans[2][3];
  
  dist = (x1-x2)*(x1-x2)+(y1-y2)*(y1-y2)+(z1-z2)*(z1-z2);
  
  printf("Dist = %3.2f\n",dist);
  if(dist < collide_dist)
    return 1;
   else 
    return 0;
}
static void init( void )
{
  ARParam  wparam;
  
  /* open the video path */
  if( arVideoOpen( vconf ) < 0 ) exit(0);
  /* find the size of the window */
  if( arVideoInqSize(&xsize, &ysize) < 0 ) exit(0);
  printf("Image size (x,y) = (%d,%d)\n", xsize, ysize);
  
  /* set the initial camera parameters */
  if( arParamLoad(cparam_name, 1, &wparam) < 0 ) {
    printf("Camera parameter load error !!\n");
    exit(0);
  }
  arParamChangeSize( &wparam, xsize, ysize, &cparam );
  arInitCparam( &cparam );
  printf("*** Camera Parameter ***\n");
  arParamDisp( &cparam );
  
  /* load in the object data - trained markers and associated bitmap files */
  if( (object=read_ObjData(model_name, &objectnum)) == NULL ) exit(0);
  printf("Objectfile num = %d\n", objectnum);
  
  /* open the graphics window */
  argInit( &cparam, 2.0, 0, 0, 0, 0 );
}

/* cleanup function called when program exits */
static void cleanup(void)
{
  arVideoCapStop();
  arVideoClose();
  argCleanup();
}

/* draw the the AR objects */
static int draw( ObjectData_T *object, int objectnum )
{
  int     i;
  double  gl_para[16];
  
  glClearDepth( 1.0 );
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_LIGHTING);
  /* calculate the viewing parameters - gl_para */
  for( i = 0; i < objectnum; i++ ) {
    if( object[i].visible == 0 ) continue;
    argConvGlpara(object[i].trans, gl_para);
    draw_object( object[i].id, gl_para, object[i].collide );
  }
  
  glDisable( GL_LIGHTING );
  glDisable( GL_DEPTH_TEST );
  
  return(0);
}

/* draw the user object */
static int  draw_object( int obj_id, double gl_para[16], int collide_flag )
{
  GLfloat material_negro[] = {0.0, 0.0, 0.0, 1.0};
  GLfloat material_rojo[] = {1.0, 0.0, 0.0, 1.0};
  GLfloat material_verde[] = {0.0,1.0,0.0,1.0};
  GLfloat material_azul[] = {0.0,0.0,1.0,1.0};
  GLfloat material_amarillo[] = {1.0,1.0,0.0,1.0};
  GLfloat material_magenta[] = {1.0,0.0,1.0,1.0};
  GLfloat material_cian[] = {0.0,1.0,1.0,1.0};
  GLfloat material_blanco[] = {1.0,1.0,1.0,1.0};

  GLfloat   light_position[]  = {100.0,100.0,200.0,0.0};
  GLfloat   ambi[]            = {0.1, 0.1, 0.1, 0.1};
  GLfloat   lightZeroColor[]  = {0.1, 0.1, 0.1, 0.1};
  
  argDrawMode3D();
  argDraw3dCamera( 0, 0 );
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixd( gl_para );
  
  /* set the material */
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glLightfv(GL_LIGHT0, GL_POSITION, light_position);
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambi);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, lightZeroColor);
  
  glMaterialfv(GL_FRONT, GL_SHININESS, material_rojo);	
  /* glutStrokeString(GLUT_STROKE_MONO_ROMAN, "HOLA"); */
  switch(collide_flag){
  case 1:
    if (object[4].visible && object[5].visible) {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_blanco);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_blanco);
      /* glutBitmapString(GLUT_BITMAP_TIMES_ROMAN_24, "Blanco"); */

    } else if(object[4].visible) {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_amarillo);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_amarillo);

    } else if (object[5].visible) {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_magenta);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_magenta);
    } else {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_rojo);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_rojo);
      /* glRasterPos3f(50, 50, 50); */
      /* glutBitmapString(GLUT_BITMAP_TIMES_ROMAN_24, "Rojo"); */
    }
    glTranslatef(0.0,0.0,30.0);
    if (obj_id == 0) {
      glutSolidCube(60);
    } else if (obj_id == 1) {
      glutSolidSphere(30,12,6);
    } else if (obj_id == 2) {
      glutSolidCone(30, 60, 12, 6);
    }
    break;
  case 2:
    if (object[3].visible && object[5].visible) {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_blanco);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_blanco);      
    } else if (object[3].visible) {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_amarillo);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_amarillo);
    } else if (object[5].visible) {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_cian);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_cian);
    } else {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_verde);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_verde);
    }
    glTranslatef(0.0,0.0,30.0);
    if (obj_id == 0) {
      glutSolidCube(60);
    } else if (obj_id == 1) {
      glutSolidSphere(30,12,6);
    } else if (obj_id == 2) {
      glutSolidCone(30, 60, 12, 6);
    }
    break;
  case 3:
    if (object[3].visible && object[4].visible) {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_blanco);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_blanco);      
    } else if (object[3].visible) {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_magenta);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_magenta);
    } else if (object[4].visible) {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_cian);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_cian);
    } else {
      glMaterialfv(GL_FRONT, GL_SPECULAR,material_azul);
      glMaterialfv(GL_FRONT, GL_AMBIENT, material_azul);
    }
    glTranslatef(0.0,0.0,30.0);
    if (obj_id == 0) {
      glutSolidCube(60);
    } else if(obj_id == 1) {
      glutSolidSphere(30,12,6);
    } else if (obj_id == 2) {
      glutSolidCone(30, 60, 12, 6);
    }
    break;
  case 4:
    glMaterialfv(GL_FRONT, GL_SPECULAR,material_amarillo);
    glMaterialfv(GL_FRONT, GL_AMBIENT, material_amarillo);
    glTranslatef(0.0,0.0,30.0);
    if (obj_id == 0) {
      glutSolidCube(60);
    } else if(obj_id == 1) {
      glutSolidSphere(30,12,6);
    } else if (obj_id == 2) {
      glutSolidCone(30, 60, 12, 6);
    }
  default:
    glMaterialfv(GL_FRONT, GL_SPECULAR,material_negro);
    glMaterialfv(GL_FRONT, GL_AMBIENT, material_negro);
    glTranslatef(0.0,0.0,30.0);
    if (obj_id == 0) {
      glutSolidCube(60);
    } else if (obj_id == 1) {
      glutSolidSphere(30,12,6);
    } else if (obj_id == 2) {
      glutSolidCone(30, 60, 12, 6);
    }
    break;
  }
  
  argDrawMode2D();
  
  return 0;
}
