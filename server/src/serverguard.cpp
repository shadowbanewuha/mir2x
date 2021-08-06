/*
 * =====================================================================================
 *
 *       Filename: serverguard.cpp
 *        Created: 04/26/2021 02:32:45
 *    Description:
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#include "serverguard.hpp"
#include "dbcomid.hpp"
#include "friendtype.hpp"
#include "monoserver.hpp"

extern MonoServer *g_monoServer;
ServerGuard::ServerGuard(uint32_t monID, ServerMap *mapPtr, int argX, int argY, int argDir)
    : Monster(monID, mapPtr, argX, argY, argDir, 0)
    , m_standX(argX)
    , m_standY(argY)
    , m_standDirection(argDir)
{}

corof::long_jmper ServerGuard::updateCoroFunc()
{
    uint64_t targetUID = 0;
    while(m_sdHealth.HP > 0){
        if(targetUID && !m_actorPod->checkUIDValid(targetUID)){
            targetUID = 0;
            m_inViewCOList.erase(targetUID);
        }

        if(!targetUID){
            targetUID = co_await coro_pickTarget();
        }

        if(targetUID){
            const auto [targetMapID, targetX, targetY] = co_await coro_getCOPLoc(targetUID);
            if(inView(targetMapID, targetX, targetY)){
                if(mathf::CDistance<int>(targetX, targetY, X(), Y()) == 1){
                    co_await coro_attackUID(targetUID, DBCOM_MAGICID(u8"物理攻击"));
                }
                else{
                    co_await coro_jumpAttackUID(targetUID);
                }
            }
            else{
                m_inViewCOList.erase(targetUID);
                targetUID = 0;
                co_await coro_jumpBack();
            }
        }
        else{
            co_await coro_jumpBack();
        }
        co_await corof::async_wait(200);
    }

    goDie();
    co_return true;
}

void ServerGuard::jumpBack(std::function<void()> onOK, std::function<void()> onError)
{
    if(X() == m_standX && Y() == m_standY){
        if(Direction() != m_standDirection){
            m_direction = m_standDirection;
            dispatchAction(makeActionStand());
        }

        if(onOK){
            onOK();
        }
        return;
    }

    requestJump(m_standX, m_standY, m_standDirection, onOK, onError);
}

void ServerGuard::checkFriend(uint64_t uid, std::function<void(int)> fnOp)
{
    fflassert(uid != 0);
    fflassert(uid != UID());

    switch(uidf::getUIDType(uid)){
        case UID_MON:
            {
                switch(DBCOM_MONSTERRECORD(uidf::getMonsterID(uid)).behaveMode){
                    case BM_GUARD  : fnOp(FT_FRIEND ); return;
                    case BM_NEUTRAL: fnOp(FT_NEUTRAL); return;
                    default        : fnOp(FT_ENEMY  ); return;
                }
            }
        case UID_PLY:
            {
                fnOp(FT_NEUTRAL);
                return;
            }
        default:
            {
                fnOp(FT_FRIEND);
                return;
            }
    }
}

bool ServerGuard::canMove() const
{
    return CharObject::canMove();
}

bool ServerGuard::canAttack() const
{
    if(!CharObject::canAttack()){
        return false;
    }

    if(m_lastAction != ACTION_ATTACK){
        return true;
    }
    return g_monoServer->getCurrTick() >= m_lastActionTime.at(ACTION_ATTACK) + getMR().attackWait;
}