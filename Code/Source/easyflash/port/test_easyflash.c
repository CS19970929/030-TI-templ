#include "easyflash.h"
#include <string.h>
#include <stdio.h>
#include "main.h"

#define MY_STRUCT_ENV_KEY "my_struct" // 环境变量键名
#define MY_STRUCT_VERSION 0x0100	  // 自定义版本号，例如 0x0100 表示 1.0

// 举例：一个设备配置结构体（仅作示例）
typedef struct
{
	uint16_t version;  // 2 字节：用于区分不同版本
	int32_t device_id; // 4 字节：设备 ID
	float threshold;   // 4 字节：阈值
	char name[16];	   // 16 字节：设备名称（以 '\0' 结尾）
	uint8_t flags;	   // 1 字节：一些标志位
					   // 如果以后想扩展字段，可以在末尾追加新成员
} my_struct_t;

my_struct_t g_cfg;


/**
 * @brief  从 EasyFlash 环境变量区读取 my_struct_t 结构
 * @param  p         : 指向用户提供的 my_struct_t 变量，供输出使用
 * @return EF_NO_ERR     : 读取并校验成功，p 中就是有效数据
 *         EF_ENV_NO_INIT: 环境区尚未初始化或没有写过数据
 *         EF_ENV_KEY_NOT_EXIST: 指定 key 没有数据
 *         EF_ENV_VAR_NULL   : 环境变量中空指针
 *         EF_ENV_TYPE_ERR   : Blob 格式或版本号不匹配
 *         其他错误码       : Flash 读取失败
 */
#if 0
EfErrCode load_my_struct(my_struct_t *p)
{
	EfErrCode ret;
	uint32_t length = 0;

	if (p == NULL)
	{
		return -1;
	}
#if 0

    // 1. 先读出存储在 Flash 里的 Blob 大小
    //    ef_get_env_blob(key, buffer, &length) 有两种用法：
    //      - 如果 buffer == NULL 且 length == 0: 只返回当前 key 存在的 blob 大小到 length
    //      - 如果 buffer 不为 NULL 且 length >= blob_size: 把 blob 数据拷到 buffer 中，并更新 length
    ret = ef_get_env_blob(MY_STRUCT_ENV_KEY, NULL, sizeof(my_struct_t), &length);
    if (ret != EF_NO_ERR)
    {
        // 如果返回 EF_ENV_KEY_NOT_EXIST，说明根本没写过这条 key
        return ret;
    }
    if (length != sizeof(my_struct_t))
    {
        // 如果 Blob 大小不对，很可能是用户手动写过别的长度，或者版本不匹配／结构变化
        // return EF_ENV_TYPE_ERR;
        return -1;
    }

    // 2. 调用第二次 ef_get_env_blob 把实际数据拷贝出来
    ret = ef_get_env_blob(MY_STRUCT_ENV_KEY, (void *)p, sizeof(my_struct_t), &length);
    if (ret != EF_NO_ERR)
    {
        return ret;
    }

    // 3. 版本检测：Flash 里 version 与当前代码里 MY_STRUCT_VERSION 必须一致
    if (p->version != MY_STRUCT_VERSION)
    {
        // return EF_ENV_TYPE_ERR;
        return -1;
    }
#endif

	if (load_data(MY_STRUCT_ENV_KEY, p, sizeof(my_struct_t)) != STORAGE_OK)
	{
		store_data("bms_other_element_params", &g_other_para, sizeof(g_other_para));
		log_w("[err]Stored default OTHER_ELEMENT parameters.\n");
	}
	else
	{
		log_w("[ok]Loaded bms OTHER_ELEMENT parameters from storage.\n");
	}

	return EF_NO_ERR;
}
#endif

#if 0
void test_flash_app(void)
{
	EfErrCode ret;
	// ----- 3. 尝试从 Flash 读取结构体 -----
	// ret = load_my_struct(&g_cfg);
	if (load_data(MY_STRUCT_ENV_KEY, &g_cfg, sizeof(my_struct_t)) != STORAGE_OK)
	{
		ret = -1;
		// store_data("bms_other_element_params", &g_other_para, sizeof(g_other_para));
		// log_w("[err]Stored default OTHER_ELEMENT parameters.\n");
	}
	else
	{
		log_w("[ok]Loaded bms OTHER_ELEMENT parameters from storage.\n");
	}
	// if (ret == EF_ENV_NO_INIT || ret == EF_ENV_KEY_NOT_EXIST || ret == EF_ENV_TYPE_ERR)
	if (ret != EF_NO_ERR)
	{
		// 根本没有写过，或者版本不对，使用默认值并写入 Flash
		// my_struct_set_default(&g_cfg);
		my_struct_t default_cfg = {
			.version = 0x0100,
			.device_id = 123456789,
			.threshold = 1.234f,
			.name = "MyDevice",
		};
		ret = save_my_struct(&default_cfg);
		if (ret != EF_NO_ERR)
		{
			printf("第一次写入结构体失败: %d\r\n", ret);
			while (1)
				;
		}
		printf("使用默认配置并保存到 Flash。\r\n");
	}
	// else if (ret == EF_NO_ERR)
	else if (ret == EF_NO_ERR)
	{
		// 读取成功，可以直接使用 g_cfg
		printf("从 Flash 里读取到配置：\r\n");
		printf("  version  : 0x%04X\r\n", g_cfg.version);
		printf("  device_id: %ld\r\n", g_cfg.device_id);
		printf("  threshold: %.3f\r\n", g_cfg.threshold);
		printf("  name     : %s\r\n", g_cfg.name);
		printf("  flags    : 0x%02X\r\n", g_cfg.flags);
	}
	// else
	// {
	//     // 其他意外错误
	//     printf("读取结构体时发生错误: %d\r\n", ret);
	//     while (1)
	//         ;
	// }

	// ----- 4. 如果后面业务需要修改配置，只需修改 g_cfg 并再次 save -----
	// 例如：把 threshold 改成 2.718
	g_cfg.threshold = 2.718f;
	strcpy(g_cfg.name, "RenamedDevice");
	ret = save_my_struct(&g_cfg);
	if (ret != EF_NO_ERR)
	{
		printf("更新结构体存储失败: %d\r\n", ret);
	}
	else
	{
		printf("更新后的配置已保存。\r\n");
	}
}
#endif

void test_env(void)
{
	uint32_t i_boot_times = NULL;
	char *c_old_boot_times, c_new_boot_times[11] = {0};

	/* get the boot count number from Env */
	c_old_boot_times = ef_get_env("boot_times");
	assert_param(c_old_boot_times);
	i_boot_times = atol(c_old_boot_times);
	/* boot count +1 */
	i_boot_times++;
	printf("The system now boot %d times\n\r", i_boot_times);
	/* interger to string */
	sprintf(c_new_boot_times, "%ld", i_boot_times);
	/* set and store the boot count number to Env */
	ef_set_env("boot_times", c_new_boot_times);
	ef_save_env();
}

void get_soc_easyflash(void)
{
	uint32_t bms_version = NULL;
	uint8_t len = 0;

	len = ef_get_env_blob("bms_version", &bms_version, sizeof(bms_version), NULL);
	BSP_Printf("The system bms_version %d times\n\r", bms_version);
	if (bms_version != VERSION)
	{
		BSP_Printf("[warn]The system bms_version is not match, reset to default\n\r");
		ef_env_set_default();
		{
			bms_version = VERSION;
			ef_set_env_blob("bms_version", &bms_version, sizeof(bms_version));

			SOC_Calculate_Element_flash.u8SOC_Now = 66;
			ef_set_env_blob("bms_soc", &SOC_Calculate_Element_flash.u8SOC_Now, sizeof(SOC_Calculate_Element_flash.u8SOC_Now));

			SOC_Calculate_Element_flash.u8DSG_SOC_Int = 0;
			ef_set_env_blob("bms_DSG_SOC_Int", &SOC_Calculate_Element_flash.u8DSG_SOC_Int, sizeof(SOC_Calculate_Element_flash.u8DSG_SOC_Int));

			SOC_Calculate_Element_flash.u32Cycle_times = 1;
			ef_set_env_blob("bms_Cycle_times", &SOC_Calculate_Element_flash.u32Cycle_times, sizeof(SOC_Calculate_Element_flash.u32Cycle_times));

			SOC_Calculate_Element_flash.u32CapFactory = OtherElement.u16Soc_Ah * 3600;
			ef_set_env_blob("bms_cap", &SOC_Calculate_Element_flash.u32CapFactory, sizeof(SOC_Calculate_Element_flash.u32CapFactory));

			SOC_Calculate_Element_flash.u32CapNow = SOC_Calculate_Element_flash.u32CapFactory / 100 * SOC_Calculate_Element_flash.u8SOC_Now;
			ef_set_env_blob("bms_CapNow", &SOC_Calculate_Element_flash.u32CapNow, sizeof(SOC_Calculate_Element_flash.u32CapNow));

		}
		MCU_RESET();
		// ef_save_env();
	}
	len = ef_get_env_blob("bms_soc", &SOC_Calculate_Element_flash.u8SOC_Now, sizeof(SOC_Calculate_Element_flash.u8SOC_Now), NULL);
	// len = ef_get_env_blob("bms_soh", &SOC_Calculate_Element_flash., sizeof(SOC_Calculate_Element_flash.u8SOC_Now), NULL);
	len = ef_get_env_blob("bms_cap", &SOC_Calculate_Element_flash.u32CapFactory, sizeof(SOC_Calculate_Element_flash.u32CapFactory), NULL);
	len = ef_get_env_blob("bms_DSG_SOC_Int", &SOC_Calculate_Element_flash.u8DSG_SOC_Int, sizeof(SOC_Calculate_Element_flash.u8DSG_SOC_Int), NULL);
	len = ef_get_env_blob("bms_Cycle_times", &SOC_Calculate_Element_flash.u32Cycle_times, sizeof(SOC_Calculate_Element_flash.u32Cycle_times), NULL);
	len = ef_get_env_blob("bms_CapNow", &SOC_Calculate_Element_flash.u32CapNow, sizeof(SOC_Calculate_Element_flash.u32CapNow), NULL);
	SOC_Calculate_Element_flash.u32CapChange = 0;
	SOC_Calculate_Element_flash.u32CapFull = SOC_Calculate_Element_flash.u32CapFactory;
	// SOC_Calculate_Element_flash.u32CapNow = SOC_Calculate_Element_flash.u32CapFactory / 100 * SOC_Calculate_Element_flash.u8SOC_Now;
	printf("bms_CapNow %d\n", SOC_Calculate_Element_flash.u32CapNow);

	{
		SOC_Calculate_Element = SOC_Calculate_Element_flash;
		OtherElement.u16Soc_Ah = SOC_Calculate_Element.u32CapFactory / 360 / 10;
		// ef_print_env();
		// log_w("soc %d, soh %d, cap %d, dsg_soc %d, cycles %d, cap_now %d\n\r", soc, soh, cap, dsg_soc, cycles, cap_now);
		// BSP_Printf("soc %d, soh %d, cap %d, dsg_soc %d, cycles %d, cap_now %d\n\r", soc, soh, cap, dsg_soc, cycles, cap_now);
	}
}


#if 0
uint16_t log_index = 0;
uint16_t get_log_index()
{
	size_t len;
	uint16_t index = 0;
	ef_get_env_blob("log_index", &index, sizeof(index), &len);
	// return (len == sizeof(index)) ? index : 0;
	log_index = (len == sizeof(index)) ? index : 0;
	return log_index;
}
void save_log_index(uint16_t index)
{
	ef_set_env_blob("log_index", &index, sizeof(index));
}

void add_log_entry(const BmsLogEntry *entry)
{
	uint16_t index = get_log_index();
	char key[12] = {0};
	snprintf(key, sizeof(key), "log%03d", index); // key=log000, log001...

	ef_set_env_blob(key, entry, sizeof(BmsLogEntry));

	index = (index + 1) % LOG_MAX_COUNT;
	if (index == 0)
	{
		// print_all_logs();
	}
	save_log_index(index);
}

void print_all_logs(void)
{
	BmsLogEntry entry;
	size_t len;

	for (int i = 0; i < LOG_MAX_COUNT; i++)
	{
		char key[12];
		snprintf(key, sizeof(key), "log%03d", i);
		len = ef_get_env_blob(key, &entry, sizeof(entry), NULL);
		if (len == sizeof(entry))
		{
			BSP_Printf("[LOG%03d] Time:%u ID:%d Level:%d Cell:%d Value:%d\n",
					   i, entry.timestamp, entry.event_id, entry.level,
					   entry.cell_index, entry.value);
		}
	}
}


// BmsStatus status = {.voltage = 3400, .temperature = 25, .mos_state = 1};
BmsStatus status = {};
uint32_t sys_time100ms = 0;

void test_easyflash_app(void)
{
	if (!g_st_SysTimeFlag.bits.b1Sys1000msFlag3)
		return;

	if (!load_bms_status(&status))
	{
		status.voltage = 3333;
		status.temperature = 66;
		status.mos_state = 0x55;
		save_bms_status(&status);
	}

#if 0
	{
		BmsLogEntry bms_log;
		bms_log.timestamp  	= sys_time100ms;
		bms_log.event_id	= 1;
		bms_log.level		= 99;
		bms_log.cell_index  = 88;
		bms_log.value		= (sys_time100ms + 1);
		add_log_entry(&bms_log);
		sys_time100ms++;
	}
#endif
}
#endif