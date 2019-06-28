/*
 *	simpleVRML.c
 *
 *	Demonstration of ARToolKit with models rendered in VRML.
 *
 *  Press '?' while running for help on available key commands.
 *
 *	Copyright (c) 2002 Mark Billinghurst (MB) grof@hitl.washington.edu
 *	Copyright (c) 2004 Raphael Grasset (RG) raphael.grasset@hitlabnz.org.
 *	Copyright (c) 2004-2007 Philip Lamb (PRL) phil@eden.net.nz. 
 *	
 *	Rev		Date		Who		Changes
 *	1.0.0	????-??-??	MB		Original from ARToolKit
 *  1.0.1   2004-10-29  RG		Fix for ARToolKit 2.69.
 *  1.0.2   2004-11-30  PRL     Various fixes.
 *
 */
/*
 * 
 * This file is part of ARToolKit.
 * 
 * ARToolKit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * ARToolKit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ARToolKit; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */


// ============================================================================
//	Includes
// ============================================================================

#ifdef _WIN32
#  include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#  include <GLUT/glut.h>
#else
#  include <GL/glut.h>
#endif

#include <AR/config.h>
#include <AR/video.h>
#include <AR/param.h>			
#include <AR/ar.h>
#include <AR/gsub_lite.h>
#include <AR/arvrml.h>

#include "object.h"






#define VIEW_DISTANCE_MIN		10.0		// Obiekty o wartości poniżej 10 nie będą wyświetlane
#define VIEW_DISTANCE_MAX		10000.0		// Obiekty, których wielkość jest powyżej 10000 nie będą wyświetlane




// ustawienia parametrów monitora
static int prefWindowed = TRUE;
static int prefWidth = 1280;					
static int prefHeight = 720;				
static int prefDepth = 32;					
static int prefRefresh = 0;					


static ARUint8		*gARTImage = NULL;

// wykrywanie markera
static int			gARTThreshhold = 100;
static long			gCallCountMarkerDetect = 0;
static int			scale = 50;
static GLfloat rotatex = 0.0;
static GLfloat rotatey = 0.0;
static GLfloat rotatez = 0.0;
static GLfloat translatex = 0.0;
static GLfloat translatey = 0.0;

// rysowanie
static ARParam		gARTCparam;
static ARGL_CONTEXT_SETTINGS_REF gArglSettings = NULL;

// dane obiektu
static ObjectData_T	*gObjectData;
static int			gObjectDataCount;



static int setupCamera(const char *cparam_name, char *vconf, ARParam *cparam)
{	
    ARParam			wparam;
	int				xsize, ysize;

    
    if (arVideoOpen(vconf) < 0) {
    	fprintf(stderr, "setupCamera(): Unable to open connection to camera.\n");
    	return (FALSE);
	}
	
    
    if (arVideoInqSize(&xsize, &ysize) < 0) return (FALSE);
    fprintf(stdout, "Camera image size (x,y) = (%d,%d)\n", xsize, ysize);
	
    if (arParamLoad(cparam_name, 1, &wparam) < 0) {
		fprintf(stderr, "setupCamera(): Error loading parameter file %s for camera.\n", cparam_name);
        return (FALSE);
    }
    arParamChangeSize(&wparam, xsize, ysize, cparam);
    fprintf(stdout, "*** Camera Parameter ***\n");
    arParamDisp(cparam);
	
    arInitCparam(cparam);

	if (arVideoCapStart() != 0) {
    	fprintf(stderr, "setupCamera(): Unable to begin camera data capture.\n");
		return (FALSE);		
	}
	
	return (TRUE);
}

static int setupMarkersObjects(char *objectDataFilename, ObjectData_T **objectDataRef, int *objectDataCountRef)
{	
	
    if ((*objectDataRef = read_VRMLdata(objectDataFilename, objectDataCountRef)) == NULL) {
        fprintf(stderr, "setupMarkersObjects(): read_VRMLdata returned error !!\n");
        return (FALSE);
    }
    printf("Object count = %d\n", *objectDataCountRef);

	return (TRUE);
}


static void debugReportMode(const ARGL_CONTEXT_SETTINGS_REF arglContextSettings)
{
	if (arFittingMode == AR_FITTING_TO_INPUT) {
		fprintf(stderr, "FittingMode (Z): INPUT IMAGE\n");
	} else {
		fprintf(stderr, "FittingMode (Z): COMPENSATED IMAGE\n");
	}
	
	if (arImageProcMode == AR_IMAGE_PROC_IN_FULL) {
		fprintf(stderr, "ProcMode (X)   : FULL IMAGE\n");
	} else {
		fprintf(stderr, "ProcMode (X)   : HALF IMAGE\n");
	}
	
	if (arglDrawModeGet(arglContextSettings) == AR_DRAW_BY_GL_DRAW_PIXELS) {
		fprintf(stderr, "DrawMode (C)   : GL_DRAW_PIXELS\n");
	} else if (arglTexmapModeGet(arglContextSettings) == AR_DRAW_TEXTURE_FULL_IMAGE) {
		fprintf(stderr, "DrawMode (C)   : TEXTURE MAPPING (FULL RESOLUTION)\n");
	} else {
		fprintf(stderr, "DrawMode (C)   : TEXTURE MAPPING (HALF RESOLUTION)\n");
	}
		
	if (arTemplateMatchingMode == AR_TEMPLATE_MATCHING_COLOR) {
		fprintf(stderr, "TemplateMatchingMode (M)   : Color Template\n");
	} else {
		fprintf(stderr, "TemplateMatchingMode (M)   : BW Template\n");
	}
	
	if (arMatchingPCAMode == AR_MATCHING_WITHOUT_PCA) {
		fprintf(stderr, "MatchingPCAMode (P)   : Without PCA\n");
	} else {
		fprintf(stderr, "MatchingPCAMode (P)   : With PCA\n");
	}
}

static void cleanup(void)
{
	arglCleanup(gArglSettings);
	arVideoCapStop();
	arVideoClose();
#ifdef _WIN32
	CoUninitialize();
#endif
}

static void Keyboard(unsigned char key, int x, int y)
{
	int mode, threshChange = 0;
	int sk = 0;
	
	
	switch (key) {
		case 0x1B:						
		case 'Q':
		case 'q':
			cleanup();
			exit(0);
			break;
		case 'C':
		case 'c':
			mode = arglDrawModeGet(gArglSettings);
			if (mode == AR_DRAW_BY_GL_DRAW_PIXELS) {
				arglDrawModeSet(gArglSettings, AR_DRAW_BY_TEXTURE_MAPPING);
				arglTexmapModeSet(gArglSettings, AR_DRAW_TEXTURE_FULL_IMAGE);
			} else {
				mode = arglTexmapModeGet(gArglSettings);
				if (mode == AR_DRAW_TEXTURE_FULL_IMAGE)	arglTexmapModeSet(gArglSettings, AR_DRAW_TEXTURE_HALF_IMAGE);
				else arglDrawModeSet(gArglSettings, AR_DRAW_BY_GL_DRAW_PIXELS);
			}
			fprintf(stderr, "kamera - %f (frame/sec)\n", (double)gCallCountMarkerDetect/arUtilTimer());
			gCallCountMarkerDetect = 0;
			arUtilTimerReset();
			debugReportMode(gArglSettings);
			break;
		case '[':
			threshChange = -1;
			break;
		case ']':
			threshChange = +1;
			break;
		case 'a':
			translatex -= 2;
			break;
		case 'd':
			translatex += 2;
			break;
		case 's':
			translatey -= 2;
			break;
		case 'w':
			translatey += 2;
			break;
			case '-':
			sk = -1;
			break;
		case '+':
		case '=':
			sk = +1;
			break;
		case ',':
		case '<':
			rotatez -=1;
			break;
		case '.':
		case '>':
			rotatez +=1;
			break;

		case 'P':
		case 'p':
			arDebug = !arDebug;
			break;
		default:
			break;
	}
	if (threshChange) {
		gARTThreshhold += threshChange;
		if (gARTThreshhold < 0) gARTThreshhold = 0;
		if (gARTThreshhold > 255) gARTThreshhold = 255;
		printf("poziom progowania %d.\n", gARTThreshhold);
	}
	if (sk) {
		scale += sk;
		if (scale < 1) scale = 1;
		if (scale > 500) scale = 500;
		printf("skala zmieniona na %d.\n", scale);
	}

}

static void SpecialKeys( int key, int x, int y )
{
    switch( key )
    {
    case GLUT_KEY_LEFT:
        rotatey -= 1;
        break;
                
    case GLUT_KEY_UP:
        rotatex -= 1;
        break;        
        
    case GLUT_KEY_RIGHT:
        rotatey += 1;
        break;        
        
    case GLUT_KEY_DOWN:
        rotatex += 1;
        break;
    }
}

    
static void mainLoop(void)
{
	static int ms_prev;
	int ms;
	float s_elapsed;
	ARUint8 *image;

	ARMarkerInfo    *marker_info;					// informacja o wykrytych znacznikach
    int             marker_num;						// liczba wykrytych znaczników
    int             i, j, k;
	
	
	ms = glutGet(GLUT_ELAPSED_TIME);
	s_elapsed = (float)(ms - ms_prev) * 0.001;
	if (s_elapsed < 0.01f) return; 
	ms_prev = ms;
	
	
	arVrmlTimerUpdate();
	
	
	if ((image = arVideoGetImage()) != NULL) {
		gARTImage = image;	
		
		gCallCountMarkerDetect++; 
		
		
		if (arDetectMarker(gARTImage, gARTThreshhold, &marker_info, &marker_num) < 0) {
			exit(-1);
		}
				
		
		
		for (i = 0; i < gObjectDataCount; i++) {
		
			
			k = -1;
			for (j = 0; j < marker_num; j++) {
				if (marker_info[j].id == gObjectData[i].id) {
					if( k == -1 ) k = j;
					else if (marker_info[k].cf < marker_info[j].cf) k = j; 
				}
			}
			
			if (k != -1) {
				
				if (gObjectData[i].visible == 0) {
					arGetTransMat(&marker_info[k],
								  gObjectData[i].marker_center, gObjectData[i].marker_width,
								  gObjectData[i].trans);
				} else {
					arGetTransMatCont(&marker_info[k], gObjectData[i].trans,
									  gObjectData[i].marker_center, gObjectData[i].marker_width,
									  gObjectData[i].trans);
				}
				gObjectData[i].visible = 1;
			} else {
				gObjectData[i].visible = 0;
			}
		}
		
		
		glutPostRedisplay();
	}
}


static void Visibility(int visible)
{
	if (visible == GLUT_VISIBLE) {
		glutIdleFunc(mainLoop);
	} else {
		glutIdleFunc(NULL);
	}
}


static void Reshape(int w, int h)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	
}


static void Display(void)
{
	int i;
    GLdouble p[16];
	GLdouble m[16];
	
	
	glDrawBuffer(GL_BACK);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // czyszczenie bufora
	
	arglDispImage(gARTImage, &gARTCparam, 1.0, gArglSettings);	// powiekszenie = 1.0.
	arVideoCapNext();
	gARTImage = NULL;
				
	
	arglCameraFrustumRH(&gARTCparam, VIEW_DISTANCE_MIN, VIEW_DISTANCE_MAX, p);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(p);
	glMatrixMode(GL_MODELVIEW);
		
	
	glLoadIdentity();

	
	
	
	for (i = 0; i < gObjectDataCount; i++) {
	
		if ((gObjectData[i].visible != 0) && (gObjectData[i].vrml_id >= 0)) {
	
			
			arglCameraViewRH(gObjectData[i].trans, m, 2.0);
			glLoadMatrixd(m);

			
			glScalef( scale*0.01, scale*0.01, scale*0.01 );		//skalowanie
			glTranslatef(translatex,translatey,0.0);			//przesuwanie
			glRotatef(rotatex, 1.0, 0, 0 );						//obracanie
			glRotatef(rotatey, 0, 1.0, 0 );
			glRotatef(rotatez, 0, 0, 1.0 );
			



			/
			arVrmlDraw(gObjectData[i].vrml_id);
		}			
	}
	
	
	
	glutSwapBuffers();
}




int main(int argc, char** argv)
{
	int i;
	char glutGamemode[32];
	const char *cparam_name = "Data/camera_para.dat";
	//
	// konfiguracja kamery
	//
#ifdef _WIN32
	char			*vconf = "Data\\WDM_camera_flipV.xml";
#else
	char			*vconf = "";
#endif
	char objectDataFilename[] = "Data/object_data_vrml";
	
	

	glutInit(&argc, argv);

	

	if (!setupCamera(cparam_name, vconf, &gARTCparam)) {
		fprintf(stderr, "main(): Unable to set up AR camera.\n");
		exit(-1);
	}
	
#ifdef _WIN32
	CoInitialize(NULL);
#endif

	
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	if (!prefWindowed) {
		if (prefRefresh) sprintf(glutGamemode, "%ix%i:%i@%i", prefWidth, prefHeight, prefDepth, prefRefresh);
		else sprintf(glutGamemode, "%ix%i:%i", prefWidth, prefHeight, prefDepth);
		glutGameModeString(glutGamemode);
		glutEnterGameMode();
	} else {
		glutInitWindowSize(prefWidth, prefHeight);
		glutCreateWindow(argv[0]);
	}

	
	if ((gArglSettings = arglSetupForCurrentContext()) == NULL) {
		fprintf(stderr, "main(): arglSetupForCurrentContext() returned error.\n");
		cleanup();
		exit(-1);
	}
	debugReportMode(gArglSettings);
	glEnable(GL_DEPTH_TEST);
	arUtilTimerReset();

	if (!setupMarkersObjects(objectDataFilename, &gObjectData, &gObjectDataCount)) {
		fprintf(stderr, "main(): Unable to set up AR objects and markers.\n");
		cleanup();
		exit(-1);
	}
	
	
    fprintf(stdout, "Pre-rendering the VRML objects...");
	fflush(stdout);
    glEnable(GL_TEXTURE_2D);
    for (i = 0; i < gObjectDataCount; i++) {
		arVrmlDraw(gObjectData[i].vrml_id);
    }
    glDisable(GL_TEXTURE_2D);
	fprintf(stdout, " done\n");
	
	
	glutDisplayFunc(Display);
	glutReshapeFunc(Reshape);
	glutVisibilityFunc(Visibility);
	glutKeyboardFunc(Keyboard);
	glutSpecialFunc(SpecialKeys);
		
	glutMainLoop();

	return (0);
}
