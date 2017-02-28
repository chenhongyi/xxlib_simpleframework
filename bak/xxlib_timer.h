#pragma once
#include "xxlib_bbuffer.h"

namespace xxlib
{
	// todo: ToString support ?

	struct TimerManager;
	struct TimerBase : public MPObject
	{
		typedef MPObject BaseType;

		// TimerManager 于 Register 时填充下列成员
		TimerManager* timerManager = nullptr;	// 指向管理器
		TimerBase* prevTimer = nullptr;			// 指向同一 ticks 下的上一 timer
		TimerBase* nextTimer = nullptr;			// 指向同一 ticks 下的下一 timer
		int	timerssIndex = -1;					// 位于管理器 timerss 数组的下标

		virtual void Execute() = 0;				// 时间到达后要执行的内容( 执行后逻辑上会触发 RemoveFromManager 行为 )
		void RemoveFromManager();				// 从管理器中移除( 会触发 Release 行为 )

		virtual void ToBBuffer(BBuffer &bb) const override;
		virtual int FromBBuffer(BBuffer &bb) override;
	};

	// 基于内存池的 timer 管理器, 只支持传入继承至 TimerBase 的仿函数类
	struct TimerManager : public MPObject
	{
		typedef MPObject BaseType;

		TimerBase**				timerss;		// timer 链表数组
		int						timerssLen;		// timer 链表数组 长度
		int						cursor = 0;		// 环形游标

		// 设定时间总刻度
		explicit TimerManager(int timerssLen = 60)
		{
			Init(timerssLen);
		}
	protected:
		inline void Init(int timerssLen)
		{
			assert(timerssLen);
			this->timerssLen = timerssLen;
			auto bytesCount = sizeof(TimerBase*) * timerssLen;
			timerss = (TimerBase**)mempool().Alloc(bytesCount);
			memset(timerss, 0, bytesCount);
		}
	public:
		TimerManager(MemPool&) : timerss(nullptr), timerssLen(0) {}		// for 反序列化

		~TimerManager()
		{
			Dispose();
		}
		inline void Dispose()
		{
			if (timerss)
			{
				Clear();
				mempool().Free(timerss);
				timerss = nullptr;
			}
		}
		inline void Clear()
		{
			// 遍历所有 ticks 链表并 Release
			for (int i = 0; i < timerssLen; ++i)
			{
				auto t = timerss[i];
				while (t)
				{
					auto nt = t->nextTimer;	// 先把指向下一节点的指针取到, 免得下面 Release 了可能就取不到了
					t->Release();			// 减持 / 析构
					t = nt;					// 继续遍历下一节点
				};
			}
		}

		// 于指定 interval 所在 timers 链表处放入一个 timer( 不加持 )
		inline void AddDirect(int interval, TimerBase* t)
		{
			assert(t && interval >= 0 && interval < timerssLen);

			// 环形定位到 timers 下标
			interval += cursor;
			if (interval >= timerssLen) interval -= timerssLen;

			// 填充管理器 & 链表信息
			t->timerManager = this;
			t->prevTimer = nullptr;
			t->timerssIndex = interval;
			if (timerss[interval])				// 有就链起来
			{
				t->nextTimer = timerss[interval];
				timerss[interval]->prevTimer = t;
			}
			else
			{
				t->nextTimer = nullptr;
			}
			timerss[interval] = t;				// 成为链表头
		}

		// 于指定 interval 所在 timers 链表处放入一个 timer
		inline void Add(int interval, TimerBase* t)
		{
			t->AddRef();
			AddDirect(interval, t);
		}

		// 移除一个 timer
		inline void Remove(TimerBase* t)
		{
			assert(t->timerManager && (t->prevTimer || timerss[t->timerssIndex] == t));
			if (t->nextTimer) t->nextTimer->prevTimer = t->prevTimer;
			if (t->prevTimer) t->prevTimer->nextTimer = t->nextTimer;
			else timerss[t->timerssIndex] = t->nextTimer;
			t->timerManager = nullptr;
			t->Release();
		}

		// 触发 当前cursor 的 timers 之后 cursor++
		inline void Update()
		{
			// 遍历当前 ticks 链表并执行
			auto t = timerss[cursor];
			while (t)
			{
				t->Execute();					// 执行

				//t->timerManager = nullptr;
				auto nt = t->nextTimer;
				t->Release();
				// if (nt) nt->prevTimer = nullptr;
				t = nt;
			};

			timerss[cursor] = 0;				// 清空链表头
			cursor++;							// 环移游标
			if (cursor == timerssLen) cursor = 0;
		}

		// 触发多个 ticks 的 timerss
		void Update(int ticks)
		{
			for (int i = 0; i < ticks; ++i) Update();
		}

		// 序列化支持
		inline virtual void ToBBuffer(BBuffer &bb) const override
		{
			this->BaseType::ToBBuffer(bb);
			assert(timerss);
			bb.Write(cursor, timerssLen);
			for (int i = 0; i < timerssLen; ++i)		// 遍历编码方式为 下标:值 键值对数组
			{
				if (timerss[i]) bb.Write(i, timerss[i]);
			}
			bb.Write(-1);								// 终结符
		}
		inline virtual int FromBBuffer(BBuffer &bb) override
		{
			assert(!timerss && !timerssLen && !cursor);
			if (auto rtv = this->BaseType::FromBBuffer(bb)) return rtv;
			if (auto rtv = bb.Read(cursor, timerssLen)) return rtv;
			// todo: Limit check?
			Init(timerssLen);
			int i;
			if (auto rtv = bb.Read(i)) return rtv;
			while (i != -1)
			{
				if (auto rtv = bb.Read(timerss[i])) return rtv;
				if (auto rtv = bb.Read(i)) return rtv;
			}
			return 0;
		}
	};

	inline void TimerBase::RemoveFromManager()
	{
		assert(this->timerManager);
		this->timerManager->Remove(this);
	}

	inline void TimerBase::ToBBuffer(BBuffer &bb) const
	{
		this->BaseType::ToBBuffer(bb);
		bb.Write(timerManager, prevTimer, nextTimer);
	}
	inline int TimerBase::FromBBuffer(BBuffer &bb)
	{
		if (auto rtv = this->BaseType::FromBBuffer(bb)) return rtv;
		return bb.Read(timerManager, prevTimer, nextTimer);
	}

}
