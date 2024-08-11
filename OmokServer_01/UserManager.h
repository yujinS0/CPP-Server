#pragma once
#include <unordered_map>
#include <deque>
#include <string>
#include <vector>

#include "ErrorCode.h"
	
class User;

class UserManager 
{
public:
	UserManager();
	virtual ~UserManager();

	void Init(const int maxUserCount);

	ERROR_CODE AddUser(const int sessionIndex, const char* pszID);
	ERROR_CODE RemoveUser(const int sessionIndex); 

	std::tuple<ERROR_CODE,User*> GetUser(const int sessionIndex);

				
private:
	User* AllocUserObjPoolIndex();
	void ReleaseUserObjPoolIndex(const int index);

	User* FindUser(const int sessionIndex);
	User* FindUser(const char* pszID);
				
private:
	std::vector<User> m_UserObjPool; // 유저 객체 풀 (미리 만들고)
	std::deque<int> m_UserObjPoolIndex; // 사용하지 않는	유저 객체 인덱스

	std::unordered_map<int, User*> m_UserSessionDic; // 현재 연결된 유저 찾을 때 : 세션 인덱스로 유저 찾기
	std::unordered_map<const char*, User*> m_UserIDDic; // 로그인된 유저 찾을 때 : 유저ID로 유저 찾기

};
