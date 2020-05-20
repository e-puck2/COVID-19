/*

File    : main_e-puck2.h
Author  : Stefano Morgani
Date    : 19 May 2020
REV 1.0

Firmware to be run on the ESP32 of the e-puck2
*/

#ifndef MAIN_E_PUCK_2_H
#define MAIN_E_PUCK_2_H

#define MAX_NUM_ROBOTS 50 // Maximum numbers of robots in the simulation.

#define STATE_HEALTHY 0
#define STATE_CONTAGIOUS 1
#define STATE_INFECTED 2
#define STATE_QUARANTINE 3
#define STATE_IMMUNE 4

#define MODE_PATIENT_ZERO 0 // When selector is in position 0, then the robot acts as patient zero (start being contagious).
#define MODE_TRACING 1		// When selector is in position 15, then the robot is part of the population that does contact tracing.
#define MODE_NO_TRACING 2	// When selector is neither in position 0 nor in position 15, then the robot is part of the population that does not contact tracing.

//////////////////////////////////////////RGB_LED DEFINITIONS//////////////////////////////////////////////
#define RGB_LED2_RED_GPIO		32
#define RGB_LED2_GREEN_GPIO		33
#define RGB_LED2_BLUE_GPIO		25

#define RGB_LED4_RED_GPIO		14
#define RGB_LED4_GREEN_GPIO		27
#define RGB_LED4_BLUE_GPIO		26

#define RGB_LED6_RED_GPIO		22
#define RGB_LED6_GREEN_GPIO		21
#define RGB_LED6_BLUE_GPIO		13

#define RGB_LED8_RED_GPIO		4
#define RGB_LED8_GREEN_GPIO		16
#define RGB_LED8_BLUE_GPIO		15


///////////////////////////////////////////BUTTON DEFINITIONS//////////////////////////////////////////////
#define BUTTON_GPIO		GPIO_NUM_35


extern uint16_t robots_infected[MAX_NUM_ROBOTS];	// List of infected robots received from the server.
extern uint8_t robots_infected_index; // Number of infected robots.
extern uint8_t actor_mode; // Patient zero (tracing), tracing, no-tracing.

/**
 * @brief Set the flag indicating the robot is just being infected by a near robot.
 */
void set_robot_infected(void);

/**
 * @brief Add the detected robot id to the list of heard robots.
 */
void bt_list_add(uint16_t id);

/**
 * @brief Reset the list of heard robots.
 */
void bt_list_reset(void);

/**
 * @brief Update the list of infected robots.
 */
void infected_list_update(uint16_t *list, uint8_t num);

/**
 * @brief Flag indicating that the list of infected robots (received from the server) was just updated.
 */
void infected_list_updated(void);

#endif /* MAIN_E_PUCK_2_H */