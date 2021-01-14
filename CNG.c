/* The simulation includes both a hardware and a software component, and my partner on the project handled the hardware aspects and its programming. This is the math program that 
I have written as part of a simple simulation with simulates the behavior of an asteroid as it approaches the Earth in two dimensions. This simulation was done as a project in 
the Advanced Embedded class in the NSCC Electronic Engineering Technology program.

This project used an Arduino Nano to read the voltages present at the output of two voltages dividers. The output voltage of both voltage dividers were controlled by 
potentiometers. The output voltage from one of the voltage dividers acted as the initial angle of the asteroid and the other acted as the initial speed of the asteroid. The Arduino
was programmed to measure these voltages and send the data to a Raspberry Pi using a USB cable. The Raspberry Pi contained my math program which calculates the position of the 
asteroid with respect to the Earth based on the initial velocity components as measured by the Arduino. 

This program makes use of threading to split the calculation code into one thread and the graphical output code into another thread. I attempted to control the timing of the threads
by using empty file pointers (rough semaphores) which acted as start/stop flags, but the implementation is far from perfect. The xy position of the asteroid is calculated in the
math thread and sent to the graphics thread using a pipe where it is output to the screen.

The Advanced Embedded class served as an introduction to Linux, and computing concepts such as multitasking, threading, IPCs and semaphores using C. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> 
#include <unistd.h>
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h" 
#include "shapes.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define G 6.67408e-11 //universal graviational constant
#define Mp 5.972e24 //mass of the earth
#define Mc 26.99e9 //mass of apophis
//#define Mc 24e3 //smaller test mass
#define ts 1800 //timestep
#define pi 3.14159
#define torad pi/180
#define todeg 180/pi
#define scalar 250000

const double Px = 0; //the Earth is centered at the origin
const double Py = 0;
double Cx = -150e6; //the asteroid begins at the bottom left of Q3
double Cy = -100e6;
//double Cx = 0; //these asteroid coordinates can be used to generate a stable orbit on demand for demonstration purposes with angle = 0 and speed = 3994
//double Cy = 25e6;

float ax = 0;
float ay = 0;
float a = 0;
float v = 0;
float Fx, Fy;
float Q1xy[2];
float TempinitialArray[2];

int id[2];
int pid, error;

char InitialAngle[10];
char InitialSpeed[10];
char CurrentCx[30];
char CurrentCy[30];
char CurrentVx[20];
char CurrentVy[20];

FILE* startgfx; //1
FILE* endflag; //666
FILE* td;
FILE* TWT;

float DownSizer(double); //this function takes the asteroids coordinates in megameters and divides them by 250k for use in the 1200 by 800 drawing area 
void InitGFX(); //this function initializes the drawing area on the screen
void InitEarth(); //this function draws the Earth at the center of the drawing area 
void MoveApophis(float*); //this function moves apophis across the drawing area 
void CreatePipeAndThread(); //this function creates the unnamed pipe and threads 
void PositionCheck(); //this function checks the current position of the asteroid 
void parsestring(char*, float*);

int main()
{
	sleep(15);
	endflag = fopen("666", "w"); //while this file exists, calculations and gfx should do their thing
	fclose(endflag);
	
	float V0; //pot value needs to replace this
	float theta; //pot value needs to replace this
	float Vcx = 0;
	float Vcy = 0;
	char vt[20];
	
usleep(5000);
	
	td = fopen("Anglespeed.txt", "r");
	fscanf(td, "%s", &vt);
	fclose(td);
	TempinitialArray[0] = 0;
	TempinitialArray[1] = 0;
	parsestring(vt, TempinitialArray );
	theta = TempinitialArray[0];
	V0 = TempinitialArray[1] *1000;
	usleep(1000);
	printf("You have entered: %.1f degrees and %.1f km/s\n", theta, V0);
	printf("Computing initial x and y components...\n");
	theta *= torad;
	Vcx = V0 * (cos(theta));
	Vcy = V0 * (sin(theta));
	printf("Vcx = %.1f m/s and Vcy = %.1f m/s\n\n", Vcx, Vcy);
	float temp = theta * todeg;
	sprintf(InitialAngle, "%.1f", temp); //convert initial angle and speed to strings to display at the bottom of the drawing area 
	sprintf(InitialSpeed, "%.1f", V0);
	
	CreatePipeAndThread(); //an error in the pipe at the start causes the asteroid to skip
	
	if(pid == 0) //child process handles graphics
	{
		sleep(2); 
		startgfx = fopen("1", "r"); //if this semaphore exists, the drawing area is output to the screen and the visualization begins
		if(startgfx != NULL) 
		{
			InitGFX(); //output drawing area with initial conditions
			while(endflag != NULL) //while endflag semaphore exists, the drawing area is updated 
			{
				endflag = fopen("666", "r"); //check to see if the semaphore exists at the beginning of each loop
				int i = 0; //integer for use with strtok()
				char message[20]; //char array to store the message that is read from the pipe
				memset(message, '\0', sizeof(message)); //set char array with null terminators 
				usleep(50000);
				read(id[0], message, sizeof(message)); //read from pipe
				printf("Received: %s from the pipe\n", message);
				char* tok = strtok(message, " "); //parse message
				while (tok != NULL)
				{
					float temp = atof(tok); //temp float = the string token converted to a float
					Q1xy[i] = temp; //store x coordinate as a float first and then the y coordinate in this array
					i++; 
					tok = strtok(NULL, " ");
				}
				Q1xy[0] += 600; //shift x coordinate to Q1 equivalent
				Q1xy[1] += 600; //shift y coordinate to Q1 equivalent
				printf("Shifting coordinates to Q1 yields: x1 = %.1f and y1 = %.1f\n\n", Q1xy[0], Q1xy[1]);
				
				sprintf(CurrentCx, "%.2f", Cx); //this code converts Cx, Cy, Vx and Vy to a string to display at the bottom of the drawing area
				sprintf(CurrentCy, "%.2f", Cy); //Unfortunately I could not get them to update properly and I only show the initial values for each
				sprintf(CurrentVx, "%.2f", Vcx);
				sprintf(CurrentVy, "%.2f", Vcy);
				MoveApophis(Q1xy); //move the asteroid 
				fclose(endflag); //close endflag
			}
		}
		fclose(startgfx); //close startgfx
	}
	else //parent process handles math
	{
		while (endflag != NULL) //while endflag semaphore exists, the calculations loop continues
		{
			endflag = fopen("666", "r"); //check to see if the semaphore exists in the directory 
			double Rsq, R, xR, yR; 
			
			printf("Initial angle = %.1f deg and initial speed = %.1f m/s\n", temp, V0);
			printf("Cx-now = %g Mm and Cy-now = %g Mm\n", Cx, Cy);
			Rsq = (Cx * Cx) + (Cy * Cy); //Rsq is the square of the distance between the asteroid and the planet
			printf("Rsq = %g m^2\n", Rsq); //with the Earth at the origin, we can just use (cx * cx) + (cy * cy)
			R = sqrt(Rsq); //R is the square root of Rsq
			printf("R = %g m\n", R);
			
			xR = Cx / R; //for use in calculating the x component of the accleration
			ax = (-1 * (xR * G * Mp)) / (R*R); //x-acceleration (could have just used Rsq instead of R*R)
			yR = Cy / R; //for use in calculating the y component of the acceleration
			ay = (-1 * (yR * G * Mp)) / (R*R); //y-acceleration (could have just used Rsq instead of R*R)
			//printf("ax = %g and ay = %g\n", ax, ay);
			Vcx += (ax * ts); //here we update the x and y components of the asteroids velocity 
			Vcy += (ay * ts); 
			//printf("Vcx = %.2f and Vcy = %.2f\n", Vcx, Vcy);
			Cx += (Vcx * ts); //here we update the x and y position of the asteroid
			Cy += (Vcy * ts); 
			
			Fx = DownSizer(Cx); //here we scale down the xy positions to be compatible with the drawing area
			Fy = DownSizer(Cy);
			printf("Cx-new = %g Mm and Cy-new = %g Mm\n", Cx, Cy);
			printf("Downsizing Cx and Cy yields: %.1f and %.1f\n", Fx, Fy);
			
			char message[20]; //char array to store the string containing the converted x and y positions of the asteroid 
			memset(message, '\0', sizeof(message));
			sprintf(message, "%.1f %.1f", Fx, Fy); //convert Fx and Fy to a string separated by whitespace 
			printf("Message = %s\n", message);
			write(id[1], message, strlen(message)); //write message to pipe
			sleep(1);
			startgfx = fopen("1", "w"); //create flag to tell gfx to start the display
			sleep(1);
			fclose(startgfx); //close startgfx semaphore
			 
		}
	}

	remove("1"); //remove the startgfx semaphore from the directory before closing the program
	return 0;

}
//function definitions below
void InitGFX()
{
	unsigned int width = 1200;
	unsigned int height = 1000;
	char s[3];
	initWindowSize(50, 50, width, height);
	init(&width, &height);				// Graphics initialization
	Start(0, 0);					// Start the picture
	Background(0, 0, 0);
	Fill(175, 175, 255, 1); 
	Rect(0, 0, 1200, 200);
	
	Fill(0, 0, 0, 1);
	Text(100, 150, "Initial Angle (degrees): ", SerifTypeface, 10);
	Fill(0, 0, 0, 1);
	Text(270, 150, InitialAngle, SerifTypeface, 10);
	Fill(0, 0, 0, 1);
	Text(100, 50, "Initial Velocity (m/sec): ", SerifTypeface, 10);
	Fill(0, 0, 0, 1);
	Text(270, 50, InitialSpeed, SerifTypeface, 10);
	Fill(0, 0, 0, 1);
	Text(470, 150, "Cx (m): ", SerifTypeface, 10);
	Fill(0, 0, 0, 1);
	Text(470, 50, "Cy (m): ", SerifTypeface, 10);
	Fill(0, 0, 0, 1);
	Text(810, 150, "Vcx (m/sec): ", SerifTypeface, 10);
	Fill(0, 0, 0, 1);
	Text(810, 50, "Vcy (m/sec): ", SerifTypeface, 10);
	
	InitEarth();
}
void InitEarth()
{
	int x = 600; //0
	int y = 600; //0
	int size = 50;
	Fill(0, 0, 255, 1);
	Circle(x, y, size);
	End();
}
void MoveApophis(float* arr) 
{
	float x = Q1xy[0];
	float y = Q1xy[1];
	int size = 15;
	
	Fill(255, 0, 0, 1);
	Circle(x, y, size);
	////AreaClear(550, 150, 20, 11);
	Fill(0, 0, 0, 1);
	Text(525, 150, CurrentCx, SerifTypeface, 10);
	Fill(0, 0, 0, 1);
	Text(525, 50, CurrentCy, SerifTypeface, 10);
	Fill(0, 0, 0, 1);
	Text(900, 150, CurrentVx, SerifTypeface, 10);
	Fill(0, 0, 0, 1);
	Text(900, 50, CurrentVy, SerifTypeface, 10);
	
	End();
	sleep(1);
	PositionCheck();
}
void PositionCheck()
{
	float x = Q1xy[0];
	float y = Q1xy[1];
	if(x >=570 && x <= 620 && y >= 570 && y <= 620)
	{
		printf("************COLLISION DETECTED************\n");
		printf("************TERMINATING PROGRAM************\n\n\n");
		finish();
		remove("666");
	}
	else if(x < 0 || x > 1200)
	{
		printf("************OUT OF BOUNDS ON THE X-AXIS************\n");
		printf("************TERMINATING PROGRAM************\n\n\n");
		finish();
		remove("666");
	}
	else if(y < 200 || y > 1000)
	{
		printf("************OUT OF BOUNDS ON THE Y-AXIS************\n");
		printf("************TERMINATING PROGRAM************\n\n\n");
		finish();
		remove("666");
	}
}
float DownSizer(double d)
{
	float tempfloat;
	tempfloat = d / scalar;
	return tempfloat;
}
void CreatePipeAndThread()
{
	error = pipe(id); //create unnamed pipe
	if(error < 0) //if there is an error creating the pipe do this...
	{
		perror("pipe");
		exit(1);
	}
	pid = fork(); //create a thread
	if(pid < 0) //if thread creation fails...
	{
		perror("fork");
		exit(2);
	}
}
void parsestring(char* vt, float* TempinitialArray )
{
	int increment = 0;
	char*tok1 = strtok(vt, ",");
	
	while(tok1 != NULL)
	{
		float temp1 = atof(tok1);
		TempinitialArray[increment] = temp1;
		increment++;
		tok1 = strtok(NULL, ",");
	}
}
