// G-code to RML-1 converter for Roland CNC mills
// Tested in the Roland MDX-540, should mostly work on other
// Roland mills.
//
// All movement is relative, starting position is used as the orgin.
//
// Input is read from stdin, output on stdout, messagses on stderr

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define CIRC_RES 10 // Points per mm

//////////////////
// RML Commands //
//////////////////
// The mill is kept in relative mode, all movements are relative to the starting position.
// This results in conistant behavor, regardels of cordinate system used in the g-code.
// Implementing functions lieke circular motion in relative cordinates is quite annoying,
// so the move function takes abosolute cordiates, doing the conversion internaly.

// Spindle speed and feed rate are set directly by the g-code parser

void init() {
	printf("V60;\r\n"); // Sane default movement speed, 10 mm/s
	printf("PR;\r\n"); // Relative mode
	printf("!MC0;\r\n"); // Stop spindle
	printf("!RC15;\r\n"); // Default to 12000 RPM, the max rotational speed of my machine
}

int rml_last_x = 0, rml_last_y = 0, rml_last_z = 0;
void move(float x, float y, float z) { // Abosolute movement in mm
	// Scale to 10s of micrometers
	x *= 100; y *= 100; z *= 100; 
	// Compute movement neaded to reach position
	int dx = x - rml_last_x;
	int dy = y - rml_last_y;
	int dz = z - rml_last_z;
	rml_last_x += dx;
	rml_last_y += dy;
	rml_last_z += dz;
	// Send to mill.
	printf("Z%d,%d,%d;\r\n", (int)dx, (int)dy, (int)dz);
}

////////////////////
// G code parsing //
////////////////////

// Read a number pointed to by string, advance the pointer the first non numeric charater
int read_number(char** string) {
	float number = 0;
	float sign = 1;
	if (**string == '-') {
		sign = -1;
		(*string)++;
	}
	while (**string >= '0' && **string <= '9') {
		number *= 10;
		number += **string - '0';
		(*string)++;
	}
	return number * sign;
}


// Read non-integer numbers, inserts an implied decimal point of one is not included.
int default_decimal_places = 3;
float read_float(char** string) {
	float number = 0;
	float sign = 1;
	if (**string == '-') {
		sign = -1;
		(*string)++;
	}
	while (**string >= '0' && **string <= '9') {
		number *= 10;
		number += **string - '0';
		(*string)++;
	}
	if (**string == '.') {
		(*string)++;
		float place = .1;
		while (**string >= '0' && **string <= '9') {
			number += (**string - '0') * place;
			place /= 10;
			(*string)++;
		}
	} else {
		for (int i = 0; i < default_decimal_places; i++) number /= 10;
	}
	return number * sign;
}


// G-code cordinate system settings 
int relative = 0;
float offset_x = 0, offset_y = 0, offset_z = 0;
float last_x = 0, last_y = 0, last_z = 0;
float scale = 1; // Scale factor to convert cordinates to mm.

void gcode_move(float x, float y, float z, int change_x, int change_y, int change_z) {
	x *= scale;
	y *= scale;
	z *= scale;
	// If the G-code indicates relative cordinates, convert to absolute
	if (relative) {
		x += last_x;
		y += last_y;
		z += last_z;
	}
	if (!change_x) x = last_x;
	if (!change_y) y = last_y;
	if (!change_z) z = last_z;
	// Send to mill
	last_x = x;
	last_y = y;
	last_z = z;
	move(offset_x + x, offset_y + y, offset_z + z);
}

// Plane used for circular interpolation, 0 = x, 1 = y, 2 = z.
int circular0 = 0;
int circular1 = 1;

// dir = 1: ccw
// dir = -1: Cw 
// Have_r = 0: Use center
// Have_r = 1: Use radius
void circular(
	float x, float y, float z,
	float i, float j, float k,
	float dir,
	int have_x, int have_y, int have_z
) {
	// Find starting position
	float start[3] = {last_x, last_y, last_z};
	fprintf(stderr, "Start: %f %f %f\n", start[0], start[1], start[2]);
	// Find destination, converting to absolute if needed
	if (relative) {
		x += last_x;
		y += last_y;
		z += last_z;
	}
	float dst[3] = {last_x, last_y, last_z};
	if (have_x) dst[0] = x;
	if (have_y) dst[1] = y;
	if (have_z) dst[2] = z;
	fprintf(stderr, "Dst: %f %f %f\n", dst[0], dst[1], dst[2]);
	// Find center point
	float center[3] = {i+last_x, j+last_y, k+last_z};
	fprintf(stderr, "Center: %f %f %f\n", center[0], center[1], center[2]);
	// Interpolate
	float start_angle = atan2(start[circular0] - center[circular0], start[circular1] - center[circular1]);
	float end_angle = atan2(dst[circular0] - center[circular0], dst[circular1] - center[circular1]);
	// Ensure the angles are increaseing (for dir=1), or decreasing (for dir=-1)
	if ((end_angle - start_angle)*dir <= 0) {
		end_angle += M_PI * 2 * dir;
	}

	// Convert points to polar
	float start_distance = sqrt(pow(start[circular0] - center[circular0], 2) + pow(start[circular1] - center[circular1], 2));
	float end_distance = sqrt(pow(dst[circular0] - center[circular0], 2) + pow(dst[circular1] - center[circular1], 2));
//	fprintf(stderr, "Distances %f %f\n", start_distance, end_distance);
//	fprintf(stderr, "Angles %f %f\n", start_angle, end_angle);
	
	float delta_angle = end_angle - start_angle;
	int steps = (start_distance * 2 * M_PI) * CIRC_RES;
	if (steps < 10) steps = 10;
	fprintf(stderr, "Using %d steps\n", steps);

	// Linearly interpolate in polar cordinates
	for (int i = 0; i < steps; i++) {
		float a = (float)i / steps;
		float r = start_distance*(1-a) + end_distance*a;
//		fprintf(stderr, "R: %f\n", r);
		float angle = start_angle*(1-a) + end_angle*a;
		// Linearly interpolate for cordinates other then the plain of interpolation
		float point[3] = {
			start[0]*(1-a) + dst[0]*a,
			start[1]*(1-a) + dst[1]*a,
			start[2]*(1-a) + dst[2]*a,
		};
		point[circular0] = sin(angle) * r + center[circular0];
		point[circular1] = cos(angle) * r + center[circular1];
		// Send to mill
		point[0] *= scale;
		point[1] *= scale;
		point[2] *= scale;
		move(offset_x + point[0], offset_y + point[1], offset_z + point[2]);
	}
	// Ensure we ended up at the right point and update last_x
	dst[0] *= scale;
	dst[1] *= scale;
	dst[2] *= scale;
	move(offset_x + dst[0], offset_y + dst[1], offset_z + dst[2]);
	last_x = dst[0];
	last_y = dst[1];
	last_z = dst[2];	
}

float dir = 1; // Last used circular interpolation direction
void translate(char* command) {
	// Skip whitespace
	while (*command == ' ' || *command == '\t' || *command == '\r' || *command == '\n') command++;
	// Skip empty lines
	if (*command == 0) return;
	// Ignore start/end of program
	if (*command == '%') return;
	// Read first charater of command
	if (*command == 'G') { // General commands
		command++;
		int g_command = read_number(&command);
		// Read arguments
		int have_x = 0, have_y = 0, have_z = 0, have_i = 0, have_j = 0, have_k = 0;
		float x, y, z, i = 0, j = 0, k = 0;
		while (1) {
			if (*command == 'X') {
				command++;
				x = read_float(&command);
				have_x = 1;
			} else if (*command == 'Y') {
				command++;
				y = read_float(&command);
				have_y = 1;
			} else if (*command == 'Z') {
				command++;
				z = read_float(&command);
				have_z = 1;
			} else if (*command == 'Z') {
				command++;
				z = read_float(&command);
				have_z = 1;
			} else if (*command == 'I') {
				command++;
				i = read_float(&command);
				have_i = 1;
			} else if (*command == 'J') {
				command++;
				j = read_float(&command);
				have_j = 1;
			} else if (*command == 'R') {
				fprintf(stderr, "gcode2rml: Interpolation with explicit radius is not supported.\n");
				fprintf(stderr, "gcode2rml: Ingnoring block.\n");
				return;
			} else if (*command == 'P') { // Not used
				command++; read_float(&command);
			} else break;
		}
		// Execute command
		switch (g_command) {
			case 0: case 1: // G00 and G01, linear movement
				gcode_move(x, y, z, have_x, have_y, have_z);
				break;
			case 2: // Circular movement
				dir = -1;
				circular(x, y, z, i, j, k, dir, have_x, have_y, have_z);
				break;
			case 3: // Circular movement
				dir = 1;
				circular(x, y, z, i, j, k, dir, have_x, have_y, have_z);
				break;
			case 10: // Set offset
				if (have_x) offset_x = x*scale;
				if (have_y) offset_y = y*scale;
				if (have_z) offset_z = z*scale;
				break;
			// Plane of interpolation
			case 17: circular0 = 0; circular1 = 1; break;
			case 18: circular0 = 2; circular1 = 1; break;
			case 19: circular0 = 1; circular1 = 2; break;
			case 20: scale = 1; default_decimal_places = 3; break; // Milimeters
			case 21: scale = 25.4; default_decimal_places = 4; break; // Inches
			case 90: relative = 0; break; // Absolute movement
			case 91: relative = 1; break; // Relative movement
			default: 
				fprintf(stderr, "gcode2rml: Warning, command G%d is not supported\n", g_command);
				break;
		}
	} else if (*command == 'S') { // Set Spindle speed
		// Spindle speed is set in increments of 773 RPM (exerimentaly determined)
		// If number > 0: real speed = 400 + number * 773
		// If number = 0: real speed = 0
		command++;
		float speed = read_number(&command);
		if (speed < 100 && speed != 0) {
			fprintf(stderr, "gcode2rml: Warning, spindle speed codes are not supported.\n");
			fprintf(stderr, "gcode2rml: Spindle speed will likely be very wrong.\n");
		}
		int setting = round((speed - 400) / 772);
		if (setting == 0) setting = 1;
		printf("!RC%d;\r\n", (int)setting);
	} else if (*command == 'F') { // Set feedrate
		command++;
		float feedrate = read_float(&command) * scale;
		printf("V%.1f;\r\n", feedrate / 60);
	} else if (*command == 'X') { // Bare cordinates, assume linear movement
		int have_x = 0, have_y = 0, have_z = 0, have_j = 0, have_i = 0, have_k = 0;
		float x, y, z, j = 0, i = 0, k =0;
		while (1) {
			if (*command == 'X') {
				command++;
				x = read_float(&command);
				have_x = 1;
			} else if (*command == 'Y') {
				command++;
				y = read_float(&command);
				have_y = 1;
			} else if (*command == 'Z') {
				command++;
				z = read_float(&command);
				have_z = 1;
			} else if (*command == 'J') {
				command++;
				j = read_float(&command);
				have_j = 1;
			} else if (*command == 'I') {
				command++;
				i = read_float(&command);
				have_i = 1;
			} else if (*command == 'R') {
				fprintf(stderr, "gcode2rml: Interpolation with explicit radius is not supported.\n");
				fprintf(stderr, "gcode2rml: Ingnoring block.\n");
				return;
			} else break;
		};
		if (have_i || have_j || have_k) {
			circular(x, y, z, i, j, k, dir,  have_x, have_y, have_z);
		} else {
			gcode_move(x, y, z, have_x, have_y, have_z);
		}
	} else if (*command == 'M') { // Misc commands
		command++;
		int g_command = read_number(&command);
		switch (g_command) {
			case 0: break; // Program stop
			case 1: break; // Optional stop
			case 30: case 2: break; // End of program
			case 3: case 4: printf("!MC1;\r\n"); break; // Start spindle
			case 5: printf("!MC0;\r\n"); break; // Stop spindle
			default:
				fprintf(stderr, "gcode2rml: Warning, command M%d is not supported\n", g_command);
				break;
			
		}
	} else {
		fprintf(stderr, "gcode2rml: Unknown command '%c'.\n", *command);	
		exit(1);
	}
	// Recurse to handle any extra commands in the block
	if (*command != 0) translate(command);
}

int main(void) {
	fprintf(stderr, "gcode2rml: ready.\n");
	init();

	char gcode[1024];
	
	// Read lines and translate them to RML
	while (1) {
		if (!fgets(gcode, 1024, stdin)) {
			fprintf(stderr, "gcode2rml: Done!\n");
			return 0;
		}
		translate(gcode);
	}
}

