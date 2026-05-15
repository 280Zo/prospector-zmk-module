/* SPDX-License-Identifier: MIT */

#define DT_DRV_COMPAT avago_apds9960

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(prospector_apds9960, CONFIG_SENSOR_LOG_LEVEL);

#define APDS9960_ENABLE_REG  0x80
#define APDS9960_ENABLE_AIEN BIT(4)
#define APDS9960_ENABLE_AEN  BIT(1)
#define APDS9960_ENABLE_PON  BIT(0)

#define APDS9960_ATIME_REG     0x81
#define APDS9960_INT_AILTL_REG 0x84
#define APDS9960_INT_AIHTL_REG 0x86
#define APDS9960_PERS_REG      0x8C
#define APDS9960_CONFIG1_REG   0x8D
#define APDS9960_CONTROL_REG   0x8F
#define APDS9960_CONFIG2_REG   0x90
#define APDS9960_ID_REG        0x92
#define APDS9960_STATUS_REG    0x93
#define APDS9960_STATUS_AINT   BIT(4)
#define APDS9960_CDATAL_REG    0x94
#define APDS9960_CONFIG3_REG   0x9F
#define APDS9960_AICLEAR_REG   0xE7

#define APDS9960_CONTROL_AGAIN (BIT(0) | BIT(1))
#define APDS9960_AGAIN_4X      BIT(0)

#define APDS9960_ID_1 0xAB
#define APDS9960_ID_2 0x9C

#define APDS9960_DEFAULT_ATIME   219
#define APDS9960_DEFAULT_CONFIG1 0x60
#define APDS9960_DEFAULT_CONFIG2 BIT(0)
#define APDS9960_DEFAULT_CONFIG3 BIT(4)
#define APDS9960_ALWAYS_INT_AILT 0xFFFF
#define APDS9960_ALWAYS_INT_AIHT 0
/* Safety guard: do not hang the display thread forever if the sensor or wiring fails. */
#define APDS9960_FETCH_TIMEOUT   K_SECONDS(1)

struct prospector_apds9960_config {
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec int_gpio;
};

struct prospector_apds9960_data {
	struct gpio_callback gpio_cb;
	struct k_sem data_sem;
	uint16_t sample_clear;
};

static int prospector_apds9960_setup_int(const struct prospector_apds9960_config *config,
					 bool enable)
{
	return gpio_pin_interrupt_configure_dt(&config->int_gpio,
					       enable ? GPIO_INT_EDGE_TO_ACTIVE : GPIO_INT_DISABLE);
}

static void prospector_apds9960_gpio_callback(const struct device *gpio_dev,
					      struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(gpio_dev);
	ARG_UNUSED(pins);

	struct prospector_apds9960_data *data =
		CONTAINER_OF(cb, struct prospector_apds9960_data, gpio_cb);

	k_sem_give(&data->data_sem);
}

static void prospector_apds9960_drain_sem(struct k_sem *sem)
{
	while (k_sem_take(sem, K_NO_WAIT) == 0) {
	}
}

static int prospector_apds9960_clear_als_interrupt(const struct prospector_apds9960_config *config)
{
	int rc = i2c_reg_write_byte_dt(&config->i2c, APDS9960_AICLEAR_REG, 0);

	if (rc != 0) {
		LOG_ERR("Failed to clear ALS interrupt: %d", rc);
		return rc;
	}

	return 0;
}

static int prospector_apds9960_enable_als(const struct prospector_apds9960_config *config)
{
	int rc = i2c_reg_write_byte_dt(&config->i2c, APDS9960_ENABLE_REG,
				       APDS9960_ENABLE_PON | APDS9960_ENABLE_AEN |
					       APDS9960_ENABLE_AIEN);

	if (rc != 0) {
		LOG_ERR("Failed to enable ALS interrupt mode: %d", rc);
		return rc;
	}

	return 0;
}

static int prospector_apds9960_disable_sensor(const struct prospector_apds9960_config *config)
{
	int rc = i2c_reg_write_byte_dt(&config->i2c, APDS9960_ENABLE_REG, 0);

	if (rc != 0) {
		LOG_ERR("Failed to disable APDS9960: %d", rc);
		return rc;
	}

	return 0;
}

static int prospector_apds9960_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct prospector_apds9960_config *config = dev->config;
	struct prospector_apds9960_data *data = dev->data;
	uint16_t sample;
	uint8_t status;
	int pin_state;
	int rc;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_LIGHT) {
		return -ENOTSUP;
	}

	rc = prospector_apds9960_setup_int(config, false);
	if (rc != 0) {
		LOG_ERR("Failed to disable APDS9960 GPIO interrupt: %d", rc);
		return rc;
	}

	prospector_apds9960_drain_sem(&data->data_sem);

	rc = prospector_apds9960_clear_als_interrupt(config);
	if (rc != 0) {
		return rc;
	}

	rc = prospector_apds9960_setup_int(config, true);
	if (rc != 0) {
		LOG_ERR("Failed to enable APDS9960 GPIO interrupt: %d", rc);
		return rc;
	}

	rc = prospector_apds9960_enable_als(config);
	if (rc != 0) {
		(void)prospector_apds9960_setup_int(config, false);
		return rc;
	}

	/* INT may already be active here, so there may be no new GPIO edge to wake fetch. */
	pin_state = gpio_pin_get_dt(&config->int_gpio);
	if (pin_state < 0) {
		LOG_ERR("Failed to read APDS9960 INT pin: %d", pin_state);
		(void)prospector_apds9960_setup_int(config, false);
		(void)prospector_apds9960_disable_sensor(config);
		return pin_state;
	}
	if (pin_state > 0) {
		k_sem_give(&data->data_sem);
	}

	rc = k_sem_take(&data->data_sem, APDS9960_FETCH_TIMEOUT);
	if (rc != 0) {
		LOG_ERR("Timed out waiting for APDS9960 ALS interrupt: %d", rc);
		(void)prospector_apds9960_setup_int(config, false);
		(void)prospector_apds9960_disable_sensor(config);
		return rc;
	}

	rc = prospector_apds9960_setup_int(config, false);
	if (rc != 0) {
		LOG_ERR("Failed to disable APDS9960 GPIO interrupt after fetch: %d", rc);
		return rc;
	}

	rc = i2c_reg_read_byte_dt(&config->i2c, APDS9960_STATUS_REG, &status);
	if (rc != 0) {
		LOG_ERR("Failed to read APDS9960 status: %d", rc);
		(void)prospector_apds9960_disable_sensor(config);
		return rc;
	}

	if ((status & APDS9960_STATUS_AINT) == 0) {
		LOG_ERR("APDS9960 interrupt without ALS status: 0x%02x", status);
		(void)prospector_apds9960_clear_als_interrupt(config);
		(void)prospector_apds9960_disable_sensor(config);
		return -EIO;
	}

	rc = i2c_burst_read_dt(&config->i2c, APDS9960_CDATAL_REG, (uint8_t *)&sample,
			       sizeof(sample));
	if (rc != 0) {
		LOG_ERR("Failed to read APDS9960 ALS sample: %d", rc);
		(void)prospector_apds9960_clear_als_interrupt(config);
		(void)prospector_apds9960_disable_sensor(config);
		return rc;
	}

	data->sample_clear = sys_le16_to_cpu(sample);

	rc = prospector_apds9960_clear_als_interrupt(config);
	if (rc != 0) {
		(void)prospector_apds9960_disable_sensor(config);
		return rc;
	}

	rc = prospector_apds9960_disable_sensor(config);
	if (rc != 0) {
		return rc;
	}

	return 0;
}

static int prospector_apds9960_channel_get(const struct device *dev, enum sensor_channel chan,
					   struct sensor_value *val)
{
	struct prospector_apds9960_data *data = dev->data;

	if (chan != SENSOR_CHAN_LIGHT) {
		return -ENOTSUP;
	}

	val->val1 = data->sample_clear;
	val->val2 = 0;

	return 0;
}

static int prospector_apds9960_write_u16(const struct i2c_dt_spec *i2c, uint8_t reg, uint16_t value)
{
	uint16_t le_value = sys_cpu_to_le16(value);
	int rc = i2c_burst_write_dt(i2c, reg, (uint8_t *)&le_value, sizeof(le_value));

	if (rc != 0) {
		LOG_ERR("Failed to write APDS9960 register 0x%02x: %d", reg, rc);
		return rc;
	}

	return 0;
}

static int prospector_apds9960_sensor_setup(const struct device *dev)
{
	const struct prospector_apds9960_config *config = dev->config;
	uint8_t chip_id;
	int rc;

	rc = i2c_reg_read_byte_dt(&config->i2c, APDS9960_ID_REG, &chip_id);
	if (rc != 0) {
		LOG_ERR("Failed to read APDS9960 chip id: %d", rc);
		return rc;
	}

	if (chip_id != APDS9960_ID_1 && chip_id != APDS9960_ID_2) {
		LOG_ERR("Invalid APDS9960 chip id: 0x%02x", chip_id);
		return -ENODEV;
	}

	rc = prospector_apds9960_disable_sensor(config);
	if (rc != 0) {
		return rc;
	}

	rc = prospector_apds9960_clear_als_interrupt(config);
	if (rc != 0) {
		return rc;
	}

	rc = i2c_reg_write_byte_dt(&config->i2c, APDS9960_ATIME_REG, APDS9960_DEFAULT_ATIME);
	if (rc != 0) {
		LOG_ERR("Failed to configure APDS9960 ALS integration time: %d", rc);
		return rc;
	}

	rc = i2c_reg_write_byte_dt(&config->i2c, APDS9960_CONFIG1_REG, APDS9960_DEFAULT_CONFIG1);
	if (rc != 0) {
		LOG_ERR("Failed to configure APDS9960 CONFIG1: %d", rc);
		return rc;
	}

	rc = i2c_reg_write_byte_dt(&config->i2c, APDS9960_CONFIG2_REG, APDS9960_DEFAULT_CONFIG2);
	if (rc != 0) {
		LOG_ERR("Failed to configure APDS9960 CONFIG2: %d", rc);
		return rc;
	}

	rc = i2c_reg_write_byte_dt(&config->i2c, APDS9960_CONFIG3_REG, APDS9960_DEFAULT_CONFIG3);
	if (rc != 0) {
		LOG_ERR("Failed to configure APDS9960 CONFIG3: %d", rc);
		return rc;
	}

	rc = i2c_reg_write_byte_dt(&config->i2c, APDS9960_PERS_REG, 0);
	if (rc != 0) {
		LOG_ERR("Failed to configure APDS9960 ALS persistence: %d", rc);
		return rc;
	}

	rc = i2c_reg_update_byte_dt(&config->i2c, APDS9960_CONTROL_REG, APDS9960_CONTROL_AGAIN,
				    APDS9960_AGAIN_4X);
	if (rc != 0) {
		LOG_ERR("Failed to configure APDS9960 ALS gain: %d", rc);
		return rc;
	}

	rc = prospector_apds9960_write_u16(&config->i2c, APDS9960_INT_AILTL_REG,
					   APDS9960_ALWAYS_INT_AILT);
	if (rc != 0) {
		return rc;
	}

	rc = prospector_apds9960_write_u16(&config->i2c, APDS9960_INT_AIHTL_REG,
					   APDS9960_ALWAYS_INT_AIHT);
	if (rc != 0) {
		return rc;
	}

	return 0;
}

static int prospector_apds9960_init(const struct device *dev)
{
	const struct prospector_apds9960_config *config = dev->config;
	struct prospector_apds9960_data *data = dev->data;
	int rc;

	if (!device_is_ready(config->i2c.bus)) {
		LOG_ERR("APDS9960 I2C bus is not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&config->int_gpio)) {
		LOG_ERR("APDS9960 INT GPIO is not ready");
		return -ENODEV;
	}

	memset(data, 0, sizeof(*data));
	k_sem_init(&data->data_sem, 0, K_SEM_MAX_LIMIT);

	rc = gpio_pin_configure_dt(&config->int_gpio, GPIO_INPUT | config->int_gpio.dt_flags);
	if (rc != 0) {
		LOG_ERR("Failed to configure APDS9960 INT GPIO: %d", rc);
		return rc;
	}

	gpio_init_callback(&data->gpio_cb, prospector_apds9960_gpio_callback,
			   BIT(config->int_gpio.pin));

	rc = gpio_add_callback(config->int_gpio.port, &data->gpio_cb);
	if (rc != 0) {
		LOG_ERR("Failed to add APDS9960 GPIO callback: %d", rc);
		return rc;
	}

	rc = prospector_apds9960_setup_int(config, false);
	if (rc != 0) {
		LOG_ERR("Failed to initialize APDS9960 GPIO interrupt: %d", rc);
		return rc;
	}

	return prospector_apds9960_sensor_setup(dev);
}

static DEVICE_API(sensor, prospector_apds9960_driver_api) = {
	.sample_fetch = prospector_apds9960_sample_fetch,
	.channel_get = prospector_apds9960_channel_get,
};

#define PROSPECTOR_APDS9960_DEFINE(inst)                                                           \
	static struct prospector_apds9960_data prospector_apds9960_data_##inst;                    \
                                                                                                   \
	static const struct prospector_apds9960_config prospector_apds9960_config_##inst = {       \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, int_gpios),                                \
	};                                                                                         \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(                                                              \
		inst, prospector_apds9960_init, NULL, &prospector_apds9960_data_##inst,            \
		&prospector_apds9960_config_##inst, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,      \
		&prospector_apds9960_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PROSPECTOR_APDS9960_DEFINE)
