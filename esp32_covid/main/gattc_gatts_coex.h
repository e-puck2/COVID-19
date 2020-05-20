
#define BT_NAME_NO_CHANGE 0
#define BT_NAME_CONTAGIOUS 1
#define BT_NAME_HEALTHY 2
#define BT_NAME_IMMUNE 3

/**
 * @brief Init the BLE communication (scanner and advertiser).
 */
void init_ble(void);

/**
 * @brief Start BLE scanning.
 */
void start_scanning(void);

/**
 * @brief Stop BLE scanning.
 */
void stop_scanning(void);

/**
 * @brief Start BLE advertisement.
 */
void start_advertising(void);

/**
 * @brief Stop BLE advertisement.
 */
void stop_advertising(void);

/**
 * @brief Return the robot ID read from the flash.
 * @return robot ID.
 */
uint16_t get_robot_id(void);

/**
 * @brief Change the BLE advertisement device name.
 */
void set_bt_name(uint8_t value);