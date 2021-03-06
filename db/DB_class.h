﻿#include <xx_bbuffer.h>

namespace DB
{
    // 对应 account 账号表
    struct Account;
    // 对应 account 账号表
    struct Account : xx::MPObject
    {
        // 自增主键
        int64_t id = 0;
        // 用户名( 唯一索引 )
        xx::String_p username;
        // 密码( 无索引 )
        xx::String_p password;

        typedef Account ThisType;
        typedef xx::MPObject BaseType;
	    Account();
		Account(Account const&) = delete;
		Account& operator=(Account const&) = delete;
        virtual void ToString(xx::String &str) const override;
        virtual void ToStringCore(xx::String &str) const override;
    };
    using Account_p = xx::Ptr<Account>;
	using Account_v = xx::Dock<Account>;


	inline Account::Account()
	{
	}

    inline void Account::ToString(xx::String &str) const
    {
        if (tsFlags())
        {
        	str.Append("[ \"***** recursived *****\" ]");
        	return;
        }
        else tsFlags() = 1;

        str.Append("{ \"type\" : \"Account\"");
        ToStringCore(str);
        str.Append(" }");
        
        tsFlags() = 0;
    }
    inline void Account::ToStringCore(xx::String &str) const
    {
        this->BaseType::ToStringCore(str);
        str.Append(", \"id\" : ", this->id);
        str.Append(", \"username\" : ", this->username);
        str.Append(", \"password\" : ", this->password);
    }
}
namespace xx
{
	template<>
	struct MemmoveSupport<DB::Account_v>
	{
		static const bool value = true;
    };
}

namespace xx
{
}
