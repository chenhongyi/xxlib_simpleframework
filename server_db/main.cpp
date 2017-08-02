#include "xx_uv.h"
#include "xx_helpers.h"
#include "pkg\PKG_class.h"
#include <xx_sqlite.h>
#include <mutex>
#include <thread>
#include <optional>

struct Peer;
struct Service;
struct Listener;
struct TaskManager;
struct Dispacher;

/******************************************************************************/

struct Peer : xx::UVServerPeer
{
	Service* service;

	Peer(Listener* listener, Service* service);
	virtual void OnReceivePackage(xx::BBuffer & bb) override;
	virtual void OnDisconnect() override;
};

struct Listener : xx::UVListener
{
	Service* service;

	Listener(xx::UV* uv, int port, int backlog, Service* service);
	virtual xx::UVServerPeer * OnCreatePeer() override;
};

struct TaskManager : xx::MPObject
{
	Service* service;
	Dispacher* dispacher = nullptr;
	std::mutex tasksMutex;
	std::mutex resultsMutex;
	xx::Queue_v<std::function<void()>> tasks;
	xx::Queue_v<std::function<void()>> results;
	void AddTask(std::function<void()>&& f);
	void AddTask(std::function<void()> const& f);
	void AddResult(std::function<void()>&& f);
	void AddResult(std::function<void()> const& f);

	TaskManager(Service* service);
	~TaskManager();
	volatile bool running = true;
	volatile bool stoped = false;
	void ThreadProcess();
};

struct Service : xx::MPObject
{
	xx::UV_v uv;
	xx::SQLite_v sqldb;
	Listener* listener = nullptr;
	xx::MemHeaderBox<TaskManager> tm;

	Service();
	int Run();
};

struct Dispacher : xx::UVAsync
{
	TaskManager* tm;

	Dispacher(xx::UV* uv, TaskManager* tm);
	virtual void OnFire() override;
};


/******************************************************************************/

Listener::Listener(xx::UV* uv, int port, int backlog, Service* service)
	: xx::UVListener(uv, port, backlog)
	, service(service)
{
}

xx::UVServerPeer* Listener::OnCreatePeer()
{
	return mempool().Create<Peer>(this, service);
}


/******************************************************************************/


Dispacher::Dispacher(xx::UV* uv, TaskManager* tm)
	: xx::UVAsync(uv)
	, tm(tm)
{
}

void Dispacher::OnFire()
{
	while (!tm->results->Empty())
	{
		tm->results->Top()();
		tm->results->Pop();
	}
}

/******************************************************************************/

TaskManager::TaskManager(Service* service)
	: service(service)
	, tasks(mempool())
	, results(mempool())
{
	this->dispacher = service->uv->CreateAsync<Dispacher>(this);
	if (!this->dispacher) throw nullptr;
	std::thread t([this] { ThreadProcess(); });
	t.detach();
}

TaskManager::~TaskManager()
{
	running = false;
	while (!stoped) Sleep(1);
}

void TaskManager::AddTask(std::function<void()>&& f)
{
	std::lock_guard<std::mutex> lock(tasksMutex);
	tasks->Push(std::move(f));
}

void TaskManager::AddTask(std::function<void()> const& f)
{
	std::lock_guard<std::mutex> lock(tasksMutex);
	tasks->Push(std::move(f));
}

void TaskManager::AddResult(std::function<void()>&& f)
{
	{
		std::lock_guard<std::mutex> lock(resultsMutex);
		results->Push(std::move(f));
	}
	dispacher->Fire();
}

void TaskManager::AddResult(std::function<void()> const& f)
{
	{
		std::lock_guard<std::mutex> lock(resultsMutex);
		results->Push(std::move(f));
	}
	dispacher->Fire();
}

void TaskManager::ThreadProcess()
{
	std::function<void()> func;
	while (running)
	{
		bool gotFunc = false;
		{
			std::lock_guard<std::mutex> lock(tasksMutex);
			gotFunc = tasks->TryPop(func);
		}
		if (gotFunc) func();
		else Sleep(1);
	}
	stoped = true;
}

/******************************************************************************/

Service::Service()
	: uv(mempool())
	, sqldb(mempool(), "data.db")
	, tm(mempool(), this)
{
	sqldb->SetPragmas(xx::SQLiteJournalModes::WAL);
}

int Service::Run()
{
	// todo: ��� db ���ǲ����½���, �Ǿ�ִ�н���ű�
	// todo: ��ȥдһ�������� for sqlite

	listener = uv->CreateListener<Listener>(12345, 128, this);
	if (!listener) return -1;
	uv->Run();
	return 0;
}

/******************************************************************************/

Peer::Peer(Listener* listener, Service* service)
	: xx::UVServerPeer(listener)
	, service(service)
{
}

void Peer::OnDisconnect() {}

void Peer::OnReceivePackage(xx::BBuffer& bb)
{
	// todo: �յ���, ����, ����������ѹ����, ת����̨�߳�ִ��
	// SQLite �߶������ڴ��, �����̵߳ķ���

	// �ڴ���� & ��������( ���� SQLite ֻռ 1 ��, ʹ�� mempool ����� ): 
	// 1. uv�߳� ���� SQL�߳� ��Ҫ������, �󽫴�����ѹ�� tasks
	// 2. SQL�߳� ִ���ڼ�, ���乩 uv�߳� ��������������, �󽫴�����ѹ�� results
	// 3. uv�߳� ��ȡ�������, ���� ��1��������ڴ�, �� ��2�����ڴ���պ���ѹ�� tasks
	// 4. tasks ִ�е�2��������ڴ����

	// �������̵� �� work thread �Ĳ���:
	// 1. ��һ���߳�˽�� tasks ���м� mempool, ���Ϊ TLS
	// 2. �������̵� 2 �� ��¼˽�� tasks ָ�� �����´���
	// 3. �������̵� 3 �� �� ˽�� tasks ѹ�ڴ���պ���
	// 4. �����̳߳���ɨ���� tasks ����, ��ɨ˽�� tasks, ɨ����ִ��

	// ���׷�������������, �����ڴ�ػ���ıȽϴ�, �����ʵ���. 



	service->tm->AddTask([service = this->service, peer = xx::MPtr<Peer>(this)/*, args*/]	// ת�� SQL �߳�
	{
		// ִ�� SQL ���, �õ����
		// auto rtv = sqlfs.execxxxxx( args... )

		service->tm->AddResult([service, peer/*, args, rtv */]	// ת�� uv �߳�
		{
		// args->Release();
		// handle( rtv )
		if (peer && peer->state == xx::UVPeerStates::Connected)	// ��� peer ������, ��һЩ�ط�����
		{
			//mp->SendPackages
		}
		// service->tm->AddTask([rtv]{ rtv->Release(); })	// ת�� SQL �߳�
	});
	});
}

/******************************************************************************/










#include "db\DB_class.h"

namespace DB
{
	// ģ�����ɵĺ�������

	struct SQLiteFuncs
	{
		xx::SQLite* sqlite;
		xx::MemPool& mp;
		xx::SQLiteString_v s;
		bool hasError = false;
		int const& lastErrorCode() { return sqlite->lastErrorCode; }
		const char* const& lastErrorMessage() { return sqlite->lastErrorMessage; }

		SQLiteFuncs(xx::SQLite* sqlite) : sqlite(sqlite), mp(sqlite->mempool()), s(mp) {}

		xx::SQLiteQuery_p query_CreateAccountTable;
		void CreateAccountTable()
		{
			hasError = true;
			auto& q = query_CreateAccountTable;
			if (!q)
			{
				q = sqlite->CreateQuery(R"=-=(
create table [account]
(
    [id] integer primary key autoincrement, 
    [username] text(64) not null unique, 
    [password] text(64) not null
)
)=-=");
			}
			if (!q) return;
			if (!q->Execute()) return;
			hasError = false;
		}

		xx::SQLiteQuery_p query_AddAccount;
		void AddAccount(DB::Account const* const& a)
		{
			assert(a);
			hasError = true;
			auto& q = query_AddAccount;
			if (!q)
			{
				q = sqlite->CreateQuery(R"=-=(
insert into [account] ([username], [password])
values (?, ?)
)=-=");
			}
			if (!q) return;
			if (q->SetParameters(a->username, a->password)) return;
			if (!q->Execute()) return;
			hasError = false;
		}

		void AddAccount2(char const* const& username, char const* const& password)
		{
			hasError = true;
			auto& q = query_AddAccount;
			if (!q)
			{
				q = sqlite->CreateQuery(R"=-=(
insert into [account] ([username], [password])
values (?, ?)
)=-=");
			}
			if (!q) return;
			if (q->SetParameters(username, password)) return;
			if (!q->Execute()) return;
			hasError = false;
		}

		xx::SQLiteQuery_p query_GetAccountByUsername;
		Account_p GetAccountByUsername(char const* const& username)
		{
			hasError = true;
			Account_p rtv;
			auto& q = query_GetAccountByUsername;
			if (!q)
			{
				q = sqlite->CreateQuery(R"=-=(
select [id], [username], [password]
  from [account]
 where [username] = ?
)=-=");
			}
			if (!q) return rtv;
			if (q->SetParameters(username)) return rtv;
			if (!q->Execute([&](xx::SQLiteReader& sr)
			{
				rtv.Create(mp);
				rtv->id = sr.ReadInt64(0);
				rtv->username.Create(mp)->Assign(sr.ReadString(1));	// ����ᴴ��Ĭ��ʵ��
				rtv->password.Create(mp, sr.ReadString(2));			// �����, ֵһ���ǿ�, ����ȷ�� Create ִ��
			})) return rtv;
			hasError = false;
			return rtv;
		}

		xx::SQLiteQuery_p query_GetAccountsByUsernames;
		xx::List_p<Account_p> GetAccountsByUsernames(xx::List_p<xx::String_p> const& usernames)
		{
			hasError = true;
			xx::List_p<Account_p> rtv;
			auto& q = query_GetAccountsByUsernames;
			{
				s->Clear();
				s->Append(R"=-=(
select [id], [username], [password]
  from [account]
 where [username] in )=-=");
				s->SQLAppend(usernames);
				q = sqlite->CreateQuery(s->C_str(), s->dataLen);
			}
			if (!q) return rtv;
			mp.CreateTo(rtv);
			if (!q->Execute([&](xx::SQLiteReader& sr)
			{
				auto& r = rtv->EmplaceMP();
				r->id = sr.ReadInt64(0);
				if (sr.IsNull(1)) r->username = nullptr; else r->username.Create(mp)->Assign(sr.ReadString(1));// ����ᴴ��Ĭ��ʵ��
				if (sr.IsNull(2)) r->password = nullptr; else r->password.Create(mp, sr.ReadString(2));
			}))
			{
				rtv = nullptr;
				return rtv;
			}
			hasError = false;
			return rtv;
		}
	};
}


int main()
{
	PKG::AllTypesRegister();
	xx::MemPool mp;

	//xx::MemHeaderBox<Service> s(mp);
	//s->Run();

	xx::SQLite_v sql(mp, "data.db");
	DB::SQLiteFuncs fs(sql);

	auto r = sql->TableExists("account");
	if (r < 0)
	{
		mp.Cout("errCode = ", sql->lastErrorCode, "errMsg = ", sql->lastErrorMessage, "\n");
		goto LabEnd;
	}
	else if (r == 0)
	{
		fs.CreateAccountTable();
		assert(!fs.hasError);

		DB::Account_p a(mp);

		a->username.Create(mp)->Assign("a");
		a->password.Create(mp)->Assign("1");
		fs.AddAccount(a);
		assert(!fs.hasError);

		fs.AddAccount2("b", "2");
		assert(!fs.hasError);
	}

	{
		auto a = fs.GetAccountByUsername("a");
		if (fs.hasError)
		{
			mp.Cout("errCode = ", fs.lastErrorCode(), "errMsg = ", fs.lastErrorMessage(), "\n");
			goto LabEnd;
		}
		else if (a == nullptr)
		{
			mp.Cout("can't find account a!\n");
		}
		else
		{
			mp.Cout("found account a! id = ", a->id, " password = ", a->password, "\n");
		}
	}
	{
		xx::List_p<xx::String_p> usernames(mp);
		usernames->EmplaceMP("a");
		usernames->EmplaceMP("b");
		auto as = fs.GetAccountsByUsernames(usernames);
		for (auto& a : *as)
		{
			mp.Cout(a, "\n");
		}
	}

LabEnd:
	std::cin.get();
	return 0;
}



//struct Foo : xx::MPObject
//{
//	xx::String_p str;
//	Foo() : str(mempool()) {}
//	Foo(Foo&&) = default;
//	Foo(Foo const&) = delete;
//	Foo& operator=(Foo const&) = delete;
//	~Foo()
//	{
//	}
//};
//using Foo_p = xx::Ptr<Foo>;
//using Foo_v = xx::MemHeaderBox<Foo>;
//namespace xx
//{
//	template<>
//	struct MemmoveSupport<Foo_v>
//	{
//		static const bool value = true;
//	};
//}
//
//Foo_p GetFoo(xx::MemPool& mp)
//{
//	Foo_p rtv;
//	rtv.Create(mp);
//	*rtv->str = "xxx";
//	return rtv;
//}
//



//{
//	xx::Dict_v<xx::String_p, xx::String_v> ss(mp);
//	ss->Add(xx::String_p(mp, "aa"), xx::String_v(mp, "2"));
//	ss->Add(xx::String_p(mp, "bbb"), xx::String_v(mp, "3"));
//	xx::String_p k(mp, "cccc");
//	xx::String_v v(mp, "4");
//	ss->Add(std::move(k), std::move(v));
//	k.Create(mp, "bbb");
//	auto idx = ss->Find(k);
//	mp.Cout(ss->ValueAt(idx));
//}

//{
//	auto foo = GetFoo(mp);
//}
//{
//	Foo_p foo;
//	foo.Create(mp);
//	*foo->str = "xx";
//	*foo.Create(mp)->str = "xxx";
//	foo = nullptr;
//}



//{
//	xx::List_v<Foo_p> foos(mp);
//	foos->Add(Foo_p(mp));
//	foos->Add(Foo_p(mp));
//}




