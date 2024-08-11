#include <algorithm>

#include "User.h"
#include "UserManager.h"


UserManager::UserManager()
{
}

UserManager::~UserManager()
{
}

void UserManager::Init(const int maxUserCount)
{
	for (int i = 0; i < maxUserCount; ++i)
	{
		User user;
		user.Init((short)i);

		m_UserObjPool.push_back(user);
		m_UserObjPoolIndex.push_back(i);
	}
}

User* UserManager::AllocUserObjPoolIndex()
{
	if (m_UserObjPoolIndex.empty()) {
		return nullptr;
	}

	int index = m_UserObjPoolIndex.front();
	m_UserObjPoolIndex.pop_front();
	return &m_UserObjPool[index];
}

void UserManager::ReleaseUserObjPoolIndex(const int index)
{
	m_UserObjPoolIndex.push_back(index);
	m_UserObjPool[index].Clear();
}

ERROR_CODE UserManager::AddUser(const int sessionIndex, const char* pszID) // 로그인 시
{
	if (FindUser(pszID) != nullptr) { // 중복 로그인 체크
		return ERROR_CODE::USER_MGR_ID_DUPLICATION;
	}

	auto pUser = AllocUserObjPoolIndex(); // 객체 풀에서 사용하지 않는 번호 인덱스 풀 가져오기 (사용하지 않는 객체 인덱스 가져오기)
	if (pUser == nullptr) { // 이미 객체 풀이 다 사용된 (다 찬) 경우
		return ERROR_CODE::USER_MGR_MAX_USER_COUNT;
	}

	pUser->Set(sessionIndex, pszID); // 유저 객체에 세션 인덱스와 아이디를 넣어줌 (즉 값 세팅)
		
	m_UserSessionDic.insert({ sessionIndex, pUser });
	m_UserIDDic.insert({ pszID, pUser });

	return ERROR_CODE::NONE;
}

ERROR_CODE UserManager::RemoveUser(const int sessionIndex) // 접속 끊어졌을 때
{
	auto pUser = FindUser(sessionIndex);

	if (pUser == nullptr) {
		return ERROR_CODE::USER_MGR_REMOVE_INVALID_SESSION;
	}

	auto index = pUser->GetIndex();
	auto pszID = pUser->GetID();

	m_UserSessionDic.erase(sessionIndex);
	m_UserIDDic.erase(pszID.c_str());
	ReleaseUserObjPoolIndex(index); // 객체 풀 반환

	return ERROR_CODE::NONE;
}

std::tuple<ERROR_CODE, User*> UserManager::GetUser(const int sessionIndex)
{
	auto pUser = FindUser(sessionIndex);

	if (pUser == nullptr) {
		return { ERROR_CODE::USER_MGR_INVALID_SESSION_INDEX, nullptr };
	}

	if (pUser->IsConfirm() == false) {
		return{ ERROR_CODE::USER_MGR_NOT_CONFIRM_USER, nullptr };
	}

	return{ ERROR_CODE::NONE, pUser };
}

User* UserManager::FindUser(const int sessionIndex)
{
	auto findIter = m_UserSessionDic.find(sessionIndex);
		
	if (findIter == m_UserSessionDic.end()) {
		return nullptr;
	}
		
	return (User*)findIter->second;
}

User* UserManager::FindUser(const char* pszID)
{
	auto findIter = m_UserIDDic.find(pszID);

	if (findIter == m_UserIDDic.end()) {
		return nullptr;
	}

	return (User*)findIter->second;
}
