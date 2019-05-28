
// BFSS  Block File Storage System 块文件存储系统
// BFSS（管理微服务） BFSS内部调用 外部透明

include "../utils/bfss_result.thrift"

namespace cpp BFSS_REGM
namespace go BFSS_REGM

//卷的信息
struct VOLUME_INFO{
    1:required string Uri		// sn的ip：port
    2:required i32 VolumeId     // 卷ID
    3:required i32 BlkCount     // 卷的块大小
    4:required i32 UsedCount    // 卷已经使用块数量
    5:optional string Desc		// 卷的描述
}

//返回 卷的信息
struct VOLUME_RESULT{
    1:required bfss_result.BFSS_RESULT Result 	// 错误代码
    2:optional VOLUME_INFO Info			// 卷的信息
}

//注册卷的相关信息
struct REGISTER_VOLUME_INFO{
    1:optional string Version  // 存储节点版本信息
    2:required string Uri     // sn的ip：port
    3:required i32 VolumeId    // 卷ID
    4:required i32 BlkCount    // 卷的块大小， 可以追加磁盘
    5:optional string Desc	   // 卷的描述
}

// 写数据 分配块的写入位置
struct ALLOCATE_BLKS_INFO{
    1:required i32 VolumeId     // 卷ID
    2:required i32 BeginIndex	// 起始块ID
    3:required i32 BeginOffset	// 块起始偏移
    4:required i32 EndIndex		// 结束块ID
    5:required i32 EndOffset	// 块结束偏移
}
//返回： 写数据 分配块的写入位置
struct ALLOCATE_BLKS_RESULT{
    1:required bfss_result.BFSS_RESULT Result 			// 错误代码
    2:optional ALLOCATE_BLKS_INFO BlksInfo;  	// 写数据分配块的写入位置
}

//消息信息 BFSS内部使用
struct BFSS_REGM_MESSAGE{
    1:optional i32 Param				// 自定义命令参数
	2:optional i32 ParamEx				// 自定义命令参数
    3:optional binary DataEx			// 自定义命令所带数据
}

//返回 自定义命令消息
struct MESSAGE_REGM_RESULT{
    1:required bfss_result.BFSS_RESULT Result	// 错误代码
    2:optional BFSS_REGM_MESSAGE BFSSMsg // 自定义消息
}

service BFSS_REGMD{

	// 函数名：GetVersion 获取服务版本信息(系统内部调用)
	// 参数：
	string GetVersion(),

	// 函数名：ManageMessage 消息管理 用于微服务之前的消息通知(系统内部调用)
	// 参数：
	// CmdId    自定义消息ID
	// Param    自定义消息命令参数
	// Data 	自定义消息数据
	// 返回值：
	//			参考 MESSAGE_RESULT 结构体定义
    MESSAGE_REGM_RESULT ManageMessage(1:i32 CmdId,2:i32 Param,3:binary Data),
	
	// 函数名：RegisterVolume 卷注册       用于存储微服务的注册
	// 参数：
	// VolumeInfo   注册卷的相关信息
	// 返回值：   
	//			    bfss_result:BFSS_RESULT结构体定义
    bfss_result.BFSS_RESULT RegisterVolume(1:REGISTER_VOLUME_INFO VolumeInfo),
	
	// 函数名：GetVolumeInfo 获取卷的信息
	// 参数：
	// VolumeId     指定卷的ID
	// 返回值：
	//				参考 VOLUME_RESULT 结构体定义
    VOLUME_RESULT GetVolumeInfo(1:i32 VolumeId),
	
    // 函数名：GetAllVolumeInfo 获取所有卷的信息
	// 参数： 		无
	// 返回值：
	//				卷信息的列表 参考 VOLUME_RESULT 结构体定义
	list<VOLUME_INFO> GetAllVolumeInfo()
	
	// 函数名：AllocateBlks 分配块写入位置 绑定文件
	// 参数：
	// size         需要分配的大小
	// 返回值：
	//          参考 ALLOCATE_BLKS_RESULT 结构体 定义
	ALLOCATE_BLKS_RESULT AllocateBlks(1:i32 size),

 	//函数名： AddBlkChip 空闲空间存数据库中
 	//参数：
 	//     参考 ALLOCATE_BLKS_RESULT 结构体 定义
 	//返回值：
 	//				参考 VOLUME_RESULT 结构体定义
	bfss_result.BFSS_RESULT AddBlkChip(1:ALLOCATE_BLKS_INFO Allocate_Blks_info),

}

