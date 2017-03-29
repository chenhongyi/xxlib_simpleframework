﻿#include "xx_luahelper.h"
#include "xx_random.h"
#include <iostream>

#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")

#ifdef _WIN32
#include <Windows.h>	// Sleep
#include <mmsystem.h>	// timeBeginPeriod at winmm.lib
#pragma comment(lib, "winmm.lib") 
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

struct UdpClient : xx::MPObject
{
	WSADATA d;
	SOCKET s = INVALID_SOCKET;
	sockaddr_in a;
	UdpClient()
	{
		WORD sockVersion = MAKEWORD(2, 2);
		if (WSAStartup(sockVersion, &d))
		{
			throw new std::exception("bad socket version.");
		}

		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s == INVALID_SOCKET)
		{
			throw new std::exception("socket error.");
		}

		unsigned long ul = 1;
		if (ioctlsocket(s, FIONBIO, (unsigned long *)&ul) == SOCKET_ERROR)
		{
			throw new std::exception("socket set non-block error.");
		}

		a.sin_family = AF_INET;
	}

	~UdpClient()
	{
		closesocket(s);
		WSACleanup();
	}

	inline void SetAddress(char const* ip, uint16_t port)
	{
		a.sin_family = AF_INET;
		a.sin_port = htons(port);
		a.sin_addr.s_addr = inet_addr(ip);
	}

	inline void Send(char const* const& buf, int const& len)
	{
		sendto(s, buf, len, 0, (struct sockaddr*)&a, (int)sizeof(a));	// 不理会是否出错
	}
};






#include "xymath.h"
XyMath xyMath;

#include "bases.h"
#include "fsmlua.h"
#include "skill.h"
#include "monster.h"
typedef xx::MemPool<
	SkillNear,
	SkillFar,
	SkillBase,
	Monster1,
	Monster2,
	MonsterBase,
	SceneObjBase,
	FSMLua,
	FSMBase,
	UpdateBase,
	Scene,
	XY
> MP;
#include "scene.h"

#include "bases.hpp"
#include "fsmlua.hpp"
#include "skill.hpp"
#include "monster.hpp"

int main()
{
	Scene scene;
	//if (auto rtv = luaL_dofile(scene.L, "init.lua"))
	//{
	//	std::cout << "err code = " << rtv << ", err msg = " << lua_tostring(scene.L, -1) << std::endl;
	//	return 0;
	//}

	scene.LoadLuaFile("scene.lua");
	if (auto rtv = scene.Run())
	{
		std::cout << "err code = " << rtv << ", err msg = " << scene.err->C_str() << std::endl;
		return 0;
	}

	return 0;
}

#ifdef _WIN32
#include <Windows.h>	// Sleep
#include <mmsystem.h>	// timeBeginPeriod at winmm.lib
#pragma comment(lib, "winmm.lib") 
#endif
#include "scene.hpp"
