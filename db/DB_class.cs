using System;
namespace DB
{
    /// <summary>
    /// 对应 account 账号表
    /// </summary>
    public partial class Account
    {
        /// <summary>
        /// 自增主键
        /// </summary>
        public long Id { get; set; }
        /// <summary>
        /// 用户名( 唯一索引 )
        /// </summary>
        public string Username { get; set; } = string.Empty;
        /// <summary>
        /// 密码( 无索引 )
        /// </summary>
        public string Password { get; set; } = string.Empty;
    }
}
