
/**
 * @brief Connect to the access point and open a TCP connection with the server.
 */
void wifi_init_sta(void);

/**
 * @brief Send the robot ID to the server in order to be added to the infected robots list (robot is infected).
 */
void alert_server(void);

/**
 * @brief Send the robot ID to the server in order to be removed from the infected robots list; this is done when the robot becomes immune.
 */
void remove_from_server(void);