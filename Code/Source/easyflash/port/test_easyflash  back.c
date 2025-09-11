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
 * @brief  保存一个 my_struct_t 结构到 EasyFlash 环境区
 * @param  p       : 指向要保存的结构体变量
 * @return EF_NO_ERR：写入成功；其他：写入失败
 */
EfErrCode save_my_struct(const my_struct_t *p)
{
	EfErrCode ret;

	if (p == NULL)
	{
		return -1;
	}

	// 1. 准备一个临时结构，先把版本号写好
	my_struct_t tmp;
	memcpy(&tmp, p, sizeof(my_struct_t));
	tmp.version = MY_STRUCT_VERSION;

	// 2. 把整个结构体当二进制 Blob 存储
	//    ef_set_env_blob(key, data, length) 会自动：
	//      - 在内部生成一个 “环境变量头” + 二进制数据
	//      - 写入 Flash（如果同一个 key 已经存在，会生成新记录并自动标记旧记录失效）
	// ret = ef_set_env_blob(MY_STRUCT_ENV_KEY, (const void *)&tmp, sizeof(my_struct_t));
	ret = ef_set_env(MY_STRUCT_ENV_KEY, (const void *)&tmp);
	if (ret != EF_NO_ERR)
	{
		// 写入失败时，可以在这里做日志或错误统计
		return ret;
	}

	return EF_NO_ERR;
}

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
	char *c_bms_version, c_bms_version_new[11] = {0};

	uint32_t soc = NULL;
	uint32_t soh = NULL;
	uint32_t cap = NULL;
	uint32_t dsg_soc = NULL;
	uint32_t cycles = NULL;
	uint32_t cap_now = NULL;
	char *c_soc, c_soc_new[11] = {0};
	char *c_soh, c_soh_new[11] = {0};
	char *c_cap, c_cap_new[11] = {0};
	char *c_dsg_soc, c_dsg_soc_new[11] = {0};
	char *c_cycles, c_cycles_new[11] = {0};
	char *c_cap_now, c_cap_now_new[11] = {0};

	c_bms_version = ef_get_env("bms_version");
	assert_param(c_bms_version);
	bms_version = atol(c_bms_version);
	// todo !!!version怎么会是
	BSP_Printf("The system bms_version %d times\n\r", bms_version);
	if (bms_version != VERSION)
	{
		BSP_Printf("[warn]The system bms_version is not match, reset to default\n\r");
		ef_env_set_default();

		bms_version = VERSION;
		sprintf(c_bms_version_new, "%ld", bms_version);
		ef_set_env("bms_version", c_bms_version_new);
		ef_save_env();
	}

	// load_data_test("bms_soc", &SOC_Calculate_Element.u8SOC_Now);

	/* get the boot count number from Env */
	c_soc = ef_get_env("bms_soc");
	assert_param(c_soc);
	soc = atol(c_soc);
	printf("The system soc %d times\n\r", soc);

	{
		c_soh = ef_get_env("bms_soh");
		assert_param(c_soh);
		soh = atol(c_soh);
		printf("The system soh %d times\n\r", soh);
	}

	/* get the boot count number from Env */
	c_cap = ef_get_env("bms_cap");
	assert_param(c_cap);
	cap = atol(c_cap);
	printf("The system now cap %d times\n\r", cap);

	c_dsg_soc = ef_get_env("bms_DSG_SOC_Int");
	assert_param(c_dsg_soc);
	dsg_soc = atol(c_dsg_soc);
	printf("The system now dsg_soc %d times\n\r", dsg_soc);

	c_cycles = ef_get_env("bms_Cycle_times");
	assert_param(c_cycles);
	cycles = atol(c_cycles);
	printf("The system now bms_Cycle_times %d times\n\r", cycles);

	c_cap_now = ef_get_env("bms_CapNow");
	assert_param(c_cap_now);
	cap_now = atol(c_cap_now);
	printf("The system now bms_CapNow %d times\n\r", cap_now);

	{
		SOC_Calculate_Element.u32CapChange = 0;
		SOC_Calculate_Element.u32CapFactory = cap;
		SOC_Calculate_Element.u32CapFull = cap;
		SOC_Calculate_Element.u8SOC_Now = soc;
		SOC_Calculate_Element.u8DSG_SOC_Int = dsg_soc;
		SOC_Calculate_Element.u32Cycle_times = cycles;
		// SOC_Calculate_Element.u32CapFull = ;

		SOC_Calculate_Element.u32CapNow = cap_now;

		SOC_Calculate_Element_flash = SOC_Calculate_Element;
		// ef_print_env();
		log_w("soc %d, soh %d, cap %d, dsg_soc %d, cycles %d, cap_now %d\n\r", soc, soh, cap, dsg_soc, cycles, cap_now);
		BSP_Printf("soc %d, soh %d, cap %d, dsg_soc %d, cycles %d, cap_now %d\n\r", soc, soh, cap, dsg_soc, cycles, cap_now);
		// printf("The system %d times\n\r", *data);
	}
	// todo log test
	{
		//		readLog(uint16_t id, uint16_t *data);
		//		id++;
		//		saveLog(uint16_t id, uint16_t *data);
	}
}

// StorageStatus load_data_test(const char *key, void *data)
StorageStatus load_data_test(const char *key, uint32_t *data)
{
	uint32_t soc = NULL;
	char *c_data;
	{
		c_data = ef_get_env(key);
		assert_param(c_data);
		*data = atol(c_data);
		/* boot count +1 */
		// printf("The system %s %d times\n\r", *key, *data);
		// printf("The system %d times\n\r", *data);
	}
}

void test_flash_app2(void)
{
	uint32_t soc = NULL;
	uint32_t soh = NULL;
	uint32_t cap = NULL;
	uint32_t dsg_soc = NULL;
	uint32_t cycles = NULL;
	uint32_t cap_now = NULL;
	char *c_soc, c_soc_new[11] = {0};
	char *c_soh, c_soh_new[11] = {0};
	char *c_cap, c_cap_new[11] = {0};
	char *c_dsg_soc, c_dsg_soc_new[11] = {0};
	char *c_cycles, c_cycles_new[11] = {0};
	char *c_cap_now, c_cap_now_new[11] = {0};

	{
		c_soh = ef_get_env("soh");
		assert_param(c_soh);
		soh = atol(c_soh);
		/* boot count +1 */
		soh++;
		printf("The system soh %d times\n\r", soh);
		/* interger to string */
		sprintf(c_soh_new, "%ld", soh);
		/* set and store the boot count number to Env */
		ef_set_env("soh", c_soh_new);
		ef_save_env();
	}

	/* get the boot count number from Env */
	c_cap = ef_get_env("cap");
	assert_param(c_cap);
	cap = atol(c_cap);
	/* boot count +1 */
	cap++;
	printf("The system now cap %d times\n\r", cap);
	/* interger to string */
	sprintf(c_cap_new, "%ld", cap);
	/* set and store the boot count number to Env */
	ef_set_env("cap", cap);
	ef_save_env();
}

static test_app(void)
{
	// if (0 == g_st_SysTimeFlag.bits.b1Sys100msFlag)
	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag1)
	{
		return;
	}

	uint32_t soc = NULL;
	uint32_t soh = NULL;
	uint32_t cap = NULL;
	char *c_soc, c_soc_new[11] = {0};
	char *c_soh, c_soh_new[11] = {0};
	char *c_cap, c_cap_new[11] = {0};

	/* get the boot count number from Env */
	c_soc = ef_get_env("soc");
	assert_param(c_soc);
	soc = atol(c_soc);
	/* boot count +1 */
	soc++;
	printf("The system soc %d times\n\r", soc);
	/* interger to string */
	sprintf(c_soc_new, "%ld", soc);
	/* set and store the boot count number to Env */
	ef_set_env("soc", c_soc_new);
	ef_save_env();
}

void test_soc_flash_logi(void)
{
	static uint8_t step = 0;
	static uint16_t current = 500;

	switch (step)
	{
	case 0:
		if (g_stCellInfoReport.SocElement.u16Soc >= 1 && g_stCellInfoReport.SocElement.u16Soc < 99)
		{
			g_stCellInfoReport.u16Ichg = current;
			g_stCellInfoReport.u16IDischg = 0;
			step = 1;
		}
		else if (g_stCellInfoReport.SocElement.u16Soc >= 99)
		{
			g_stCellInfoReport.u16IDischg = current;
			g_stCellInfoReport.u16Ichg = 0;
			step = 2;
		}
		break;
	case 1:
		if (g_stCellInfoReport.SocElement.u16Soc == 99)
		{
			g_stCellInfoReport.u16IDischg = current;
			g_stCellInfoReport.u16Ichg = 0;
			step = 2;
		}
		break;
	case 2:
		if (g_stCellInfoReport.SocElement.u16Soc == 1)
		{
			g_stCellInfoReport.u16Ichg = current;
			g_stCellInfoReport.u16IDischg = 0;
			step = 1;
		}
		break;

	default:
		break;
	}
}

void readLog(uint16_t id)
// void readLog(uint16_t id, uint16_t *data)
{
	char number_str[10];		 // 用于存放整型转字符串后的结果
	char final_str[100] = "log"; // 初始字符串（可以为空）

	sprintf(number_str, "%d", id);
	strcat(final_str, number_str);

	char *p = final_str;		 // 指向字符串的指针
	char *ptoid = number_str;
	ef_set_env(p, ptoid);
	ef_save_env();

	// ef_print_env();

	// 输出结果
	// printf("%s\n", final_str);

	// c_soc = ef_get_env("log+id");
}
