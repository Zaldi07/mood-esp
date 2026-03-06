#include "app/time_sync.h"

#include <string.h>
#include <time.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/apps/sntp.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "time_sync";
static volatile bool s_time_synced = false;

#ifndef CONFIG_CARMOOD_WIFI_SSID
#define CONFIG_CARMOOD_WIFI_SSID ""
#endif
#ifndef CONFIG_CARMOOD_WIFI_PASSWORD
#define CONFIG_CARMOOD_WIFI_PASSWORD ""
#endif
#ifndef CONFIG_CARMOOD_TIME_SYNC_ON_BOOT
#define CONFIG_CARMOOD_TIME_SYNC_ON_BOOT 1
#endif
#ifndef CONFIG_CARMOOD_TIME_SYNC_TIMEOUT_SEC
#define CONFIG_CARMOOD_TIME_SYNC_TIMEOUT_SEC 15
#endif
#ifndef CONFIG_CARMOOD_NTP_SERVER
#define CONFIG_CARMOOD_NTP_SERVER "ntp.aliyun.com"
#endif
#ifndef CONFIG_CARMOOD_TIME_TZ
#define CONFIG_CARMOOD_TIME_TZ "CST-8"
#endif

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_MAX_RETRY 10

static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  (void)arg;

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "开始连接 Wi-Fi: ssid=%s", CONFIG_CARMOOD_WIFI_SSID);
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    int reason = -1;
    if (event_data != NULL) {
      wifi_event_sta_disconnected_t *disc =
          (wifi_event_sta_disconnected_t *)event_data;
      reason = (int)disc->reason;
    }

    if (s_retry_num < WIFI_MAX_RETRY) {
      ESP_LOGW(TAG, "Wi-Fi 断开，重连中 (%d/%d), reason=%d", s_retry_num + 1,
               WIFI_MAX_RETRY, reason);
      esp_wifi_connect();
      s_retry_num++;
    } else {
      ESP_LOGW(TAG, "Wi-Fi 重连次数耗尽, reason=%d", reason);
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

static void apply_timezone(void) {
  setenv("TZ", CONFIG_CARMOOD_TIME_TZ, 1);
  tzset();
}

esp_err_t carmood_time_sync_once(void) {
#if CONFIG_CARMOOD_TIME_SYNC_ON_BOOT == 0
  return ESP_ERR_NOT_SUPPORTED;
#endif

  s_time_synced = false;

  if (strlen(CONFIG_CARMOOD_WIFI_SSID) == 0) {
    ESP_LOGW(TAG, "Wi-Fi SSID 为空，跳过校时");
    return ESP_ERR_INVALID_ARG;
  }

  apply_timezone();

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_RETURN_ON_ERROR(ret, TAG, "NVS 初始化失败");

  ret = esp_netif_init();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_RETURN_ON_ERROR(ret, TAG, "esp_netif_init 失败");
  }

  ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_RETURN_ON_ERROR(ret, TAG, "event loop 创建失败");
  }

  s_wifi_event_group = xEventGroupCreate();
  if (s_wifi_event_group == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  if (sta_netif == NULL) {
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;
    return ESP_FAIL;
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi_init 失败");

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_RETURN_ON_ERROR(
      esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                          &wifi_event_handler, NULL,
                                          &instance_any_id),
      TAG, "注册 WIFI 事件失败");
  ESP_RETURN_ON_ERROR(
      esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                          &wifi_event_handler, NULL,
                                          &instance_got_ip),
      TAG, "注册 IP 事件失败");

  wifi_config_t wifi_config = {};
  strlcpy((char *)wifi_config.sta.ssid, CONFIG_CARMOOD_WIFI_SSID,
          sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, CONFIG_CARMOOD_WIFI_PASSWORD,
          sizeof(wifi_config.sta.password));

  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set_mode 失败");
  ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG,
                      "set_config 失败");
  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi_start 失败");

  EventBits_t bits = xEventGroupWaitBits(
      s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE,
      pdMS_TO_TICKS(CONFIG_CARMOOD_TIME_SYNC_TIMEOUT_SEC * 1000));

  if ((bits & WIFI_CONNECTED_BIT) == 0) {
    ESP_LOGW(TAG, "Wi-Fi 连接失败或超时(%ds): ssid=%s，跳过校时",
             CONFIG_CARMOOD_TIME_SYNC_TIMEOUT_SEC, CONFIG_CARMOOD_WIFI_SSID);
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                          instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                          instance_any_id);
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;
    return ESP_FAIL;
  }

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, CONFIG_CARMOOD_NTP_SERVER);
  sntp_init();

  bool synced = false;
  for (int i = 0; i < CONFIG_CARMOOD_TIME_SYNC_TIMEOUT_SEC; i++) {
    time_t now = time(NULL);
    if (now >= 1704067200) {
      synced = true;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  sntp_stop();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        instance_got_ip);
  esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        instance_any_id);
  vEventGroupDelete(s_wifi_event_group);
  s_wifi_event_group = NULL;

  if (!synced) {
    ESP_LOGW(TAG, "NTP 校时超时");
    return ESP_ERR_TIMEOUT;
  }

  s_time_synced = true;
  apply_timezone();
  time_t now = time(NULL);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  ESP_LOGI(TAG, "时间同步成功: %04d-%02d-%02d %02d:%02d:%02d",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  return ESP_OK;
}

bool carmood_time_sync_is_synced(void) { return s_time_synced; }
