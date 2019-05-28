
namespace cpp bfss
namespace go bfssproject.bfss_web.bfss
// 错误代码
enum BFSS_RESULT {
    //-1 --- -49 BFSS 内部错误使用
    //-50 --- -100 外部接口使用
    //-100 --- -254 预留可选
    BFSS_DATA_READ_INCOMPLETE   = 1,    // 读操作请求的字节数过大，返回的字符数不够请求的数量。
    BFSS_DATA_WRITE_INCOMPLETE  = 2,    // 写操作发送的字符串超长，实际写入的内容不足发送的数据。
    BFSS_SUCCESS                = 0,    // 调用成功
    BFSS_UNKNOWN_ERROR          = -255, // 未知错误

    BFSS_PARAM_ERROR            = -51,  // 参数错误
    BFSS_SCHEME_ERROR           = -52,  // 配置错误

    BFSS_NO_SPACE               = -53,  // 磁盘空间不足，分配失败
    BFSS_NO_MEMORY              = -54,  // 内存不足，分配失败
    BFSS_TIMEOUT                = -55,  // 超时

    BFSS_NOTFOUND               = -60,  // 对象不存在
    BFSS_DUPLICATED             = -61,  // 对象已存在，创建失败
    BFSS_COMPLETED              = -62,  // 对象已写入完成
    BFSS_INCOMPLETED            = -63,  // 对象未写入完成

    BFSS_DATA_WRITE_FAILED      = -70,  // WRITE数据失败
    BFSS_DATA_READ_FAILED       = -71,  // READ数据失败
    BFSS_DATA_COMPLETE_FAILED   = -72,  // 写入完成时计算Hash失败
    BFSS_DATA_UNINITED          = -73,  // 块未被使用过

}
