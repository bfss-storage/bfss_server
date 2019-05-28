
// BFSS  Block Object Storage System 块对象存储系统
// BFSS微服务 API （BFSS外部）调用接口


include "../utils/bfss_result.thrift"

namespace cpp BFSS_API
namespace go bfssproject.bfss_web.BFSS_API

// 对象信息
struct OBJECT_INFO{
    1:required i32 ObjectSize       // 对象大小（最大4G）
    2:required i64 CreateTime       // 对象创建时间
    3:required i32 ObjectFlag       // 对象旗帜
    4:required string ObjectTag     // 对象标签
    5:required bool Complete        // 对象是否完成
    6:required string Hash          // 对象Hash，去重
}
//返回 对象信息
struct OBJECT_INFO_RESULT{
    1:required bfss_result.BFSS_RESULT Result       // 错误代码
    2:optional OBJECT_INFO ObjectInfo   // 对象信息
}

//返回 读对象结果
struct READ_RESULT{
    1:required bfss_result.BFSS_RESULT Result // 错误代码
    2:optional binary Data        // 读取的数据， 真实数据，包含数据真实大小
}

// 返回  获取的块密钥结果
struct BLK_KEY_RESULT{
    1:required bfss_result.BFSS_RESULT Result // 错误代码
    2:required binary BlkKey      // 块密钥
}

// 块信息
struct OBJECT_BLK_INFO{
    1:required i32 BlkSize     // 当前块在这个对象中所占的数据大小（由于块是加密的，所以这里的大小是按16字节块对齐的）
    2:required i32 DataOff     // 当前块在这个对象中的数据偏移（由于块是加密的，所以这里的偏移是按16字节块对齐的）
    3:required i32 VolumeId    // 卷ID
    4:required i32 BlkIndex    // 块的索引值
    5:required binary BlkKey   // 块对应的密钥
}

//对象块信息
struct OBJECT_BLKS_INFO{
    1:required OBJECT_INFO ObjectInfo      // 对象信息
    2:required list<OBJECT_BLK_INFO> Blks  // 块信息，一个对象需要很多块存储，跨块
}

//返回 对象块信息
struct OBJECT_BLKS_RESULT{
    1:required bfss_result.BFSS_RESULT Result               // 错误代码
    2:optional OBJECT_BLKS_INFO ObjectBlksInfo 	// 对象块的详细信息
}

//消息信息 BFSS内部使用
struct BFSS_MESSAGE{
    1:optional i32 Param            // 自定义命令参数
    2:optional i32 ParamEx          // 自定义命令参数
    3:optional binary DataEx        // 自定义命令所带数据
}

//返回 自定义命令消息
struct MESSAGE_RESULT{
    1:required bfss_result.BFSS_RESULT Result   // 错误代码
    2:optional BFSS_MESSAGE BFSSMsg // 自定义消息
}


service BFSS_APID{

    //-------------------------------------------------------------------------------------------------------
    // 函数名：GetVersion 获取服务版本信息(系统内部调用)
    // 参数：
    string GetVersion(),

    //-------------------------------------------------------------------------------------------------------
    // 函数名： CreateObject 创建一个存储对象（对外接口）
    // 参数：
    //  oid  对象id，最多128个英文字符（只允许'-','_','.',字母和数字），如果id已被使用这里会返回 BFSS_DUP_ID
    //  size 对象大小，这个值范围，必须在1-4G（byte）之间。
    //  flag 对象旗帜，高16位保留（必须置0），低16位调用者可自定义使用。
    //  tag  对象标签。调用者可以给对象打上标签（字符串必须是utf-8,最多128字节）。
    // 返回值：
    //      参考  bfss_result.BFSS_RESULT 结构体定义
    bfss_result.BFSS_RESULT CreateObject(1:string oid, 2:i32 size, 3:i32 flag, 4:string tag=""),

    //-------------------------------------------------------------------------------------------------------
    // 函数名: DeleteObject 写对象异常需要删除之前的句柄，否则再次写入失败
    // 参数：
    //  oid  对象id
    // 返回值：
    //      参考  bfss_result.BFSS_RESULT 结构体定义
    bfss_result.BFSS_RESULT DeleteObject(1:string oid);

    //-------------------------------------------------------------------------------------------------------
    // 函数名： Write 写对象（对外接口）
    // 参数：
    //  oid 对象id，同上
    //  offset 对象偏移
    //  data 数据内容
    // 返回值：
    //      参考  bfss_result.BFSS_RESULT 结构体定义
    bfss_result.BFSS_RESULT Write(1:string oid, 2:i32 offset, 3:binary data),

    //-------------------------------------------------------------------------------------------------------
    // 函数名： ResizeObject 重设对象大小(且必须比原来的ObjectSize小)，必须在CompleteObject()之前调用（对外接口）
    // 参数：
    //  oid  对象id，同上
    //  newsize 对象的实际大小
    // 返回值：
    //      参考  bfss_result.BFSS_RESULT 结构体定义
    bfss_result.BFSS_RESULT ResizeObject(1:string oid, 2:i32 newsize),

    //-------------------------------------------------------------------------------------------------------
    // 函数名： WriteComplete 写对象（对外接口）
    // 参数：
    //  oid 对象id，同上
    // 返回值：
    //      参考  bfss_result.BFSS_RESULT 结构体定义
    bfss_result.BFSS_RESULT CompleteObject(1:string oid),

    //-------------------------------------------------------------------------------------------------------
    // 函数名： GetObjectInfo 获取对象信息（对外接口）
    // 参数：
    //  oid 对象id，同上
    // 返回值：
    //      参考 OBJECT_INFO_RESULT 结构体定义
    OBJECT_INFO_RESULT GetObjectInfo(1:string oid),

    //-------------------------------------------------------------------------------------------------------
    // 函数名： ObjectLockHasHash 通过hash值、对象大小、对象头，判断对象是否存在
    // 参数：
    //  hash  对象hash值
    //  size  对象大小
    //  head  对象头16字节
    // 返回值
    //      对象存在，返回 BFSS_SUCCESS；对象不存在，返回其他
    bfss_result.BFSS_RESULT ObjectLockHasHash(1:string hash, 2:i32 size, 3:binary head),

    //-------------------------------------------------------------------------------------------------------
    // 函数名： CreateObjectLink 通过hash值、对象大小、对象头，创建一个存储对象（对外接口）
    // 参数：
    //  oid   对象id，同上
    //  hash  对象hash值
    //  size  对象大小
    //  head  对象头16字节
    //  flag  对象旗帜
    //  tag   对象标签
    // 返回值：
    //      参考  bfss_result.BFSS_RESULT 结构体定义
    bfss_result.BFSS_RESULT CreateObjectLink(1:string oid, 2:string hash, 3:i32 size, 4:binary head, 5:i32 flag, 6:string tag=""),

    //-------------------------------------------------------------------------------------------------------
    // 函数名： Read 读取对象（对外接口）
    // 参数:
    //  oid  对象id，同上
    //  size 预读数据大小
    //  offset 对象的数据偏移
    // 返回值：
    //			参考READ_RESULT结构体说明
    READ_RESULT Read(1:string oid, 2:i32 size, 3:i32 offset),

    //-------------------------------------------------------------------------------------------------------
    // 函数名： ReadBlk 读取加密对象
    // 参数：
    // oid  对象id，同上
    // size 预读数据大小，-1全部
    // offset 对象的数据偏移（16对齐）
    // 返回值：
    //			参考READ_RESULT结构体说明
    READ_RESULT ReadBlk(1:string oid, 2:i32 size, 3:i32 offset),


    //-------------------------------------------------------------------------------------------------------
    // 函数名： GetObjectBlksInfo 获取对象块信息（CDN对外接口）
    // 参数：
    // oid 对象id，同上
    // 返回值：
    //			返回OBJECT_BLKS_RESULT结构体
    OBJECT_BLKS_RESULT GetObjectBlksInfo(1:string oid),

    //-------------------------------------------------------------------------------------------------------
    //函数名： GetObjectBlkKey 获取块密钥结果（CDN对外接口 可选）
    //参数：
    // oid 对象id，同上
    // offset 对象偏移
    // 返回值：
    //			参考 BLK_KEY_RESULT 结构体定义
    BLK_KEY_RESULT GetObjectBlkKey(1:string oid, 2:i32 offset),

    //-------------------------------------------------------------------------------------------------------
    // 函数名： ManageMessage 消息管理 用于微服务之前的消息通知(系统内部调用)
    // 参数：
    // CmdId    自定义消息ID
    // Param    自定义消息命令参数
    // Data 	自定义消息数据
    // 返回值：
    //			参考 MESSAGE_RESULT 结构体定义
    MESSAGE_RESULT ManageMessage(1:i32 CmdId,2:i32 Param,3:binary Data)
}
