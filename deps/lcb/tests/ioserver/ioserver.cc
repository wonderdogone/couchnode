#include <cassert>
#include <sys/types.h>
#include "ioserver.h"
using namespace LCBTest;
using std::list;

extern "C" {
static void
server_runfunc(void *arg)
{
    TestServer *server = (TestServer *)arg;
    server->run();
}
}

void
TestServer::run()
{
    while (!closed) {
        struct sockaddr_in newaddr;
        socklen_t naddr = sizeof(newaddr);
        int newsock = accept(*lsn, (struct sockaddr *)&newaddr, &naddr);

        if (newsock == -1) {
            break;
        }

        TestConnection *newconn = new TestConnection(this, factory(newsock));
        startConnection(newconn);
    }
}

void
TestServer::startConnection(TestConnection *conn)
{
    mutex.lock();
    if (isClosed()) {
        conn->close();
        delete conn;
    } else {
        conns.push_back(conn);
    }
    mutex.unlock();
}

TestServer::TestServer()
{
    lsn = SockFD::newListener();
    closed = false;
    factory = plainSocketFactory;

    // Now spin up a thread to start the accept loop
    thr = new Thread(server_runfunc, this);
}

TestServer::~TestServer()
{
    close();
    mutex.lock();
    std::list<TestConnection*>::iterator iter = conns.begin();
    for (; iter != conns.end(); ++iter) {
        (*iter)->close();
        delete *iter;
    }
    mutex.unlock();
    thr->join();
    delete thr;
    mutex.close();
    delete lsn;
}

std::string
TestServer::getPortString()
{
    char buf[4096];
    sprintf(buf, "%d", lsn->getLocalPort());
    return std::string(buf);
}

TestConnection *
TestServer::findConnection(uint16_t port)
{
    TestConnection *ret = NULL;
    list<TestConnection *>::iterator iter;

    while (ret == NULL) {
        sched_yield();
        mutex.lock();
        iter = conns.begin();

        for (; iter != conns.end(); ++iter) {
            if ((*iter)->getPeerPort() == port) {
                ret = *iter;
                break;
            }
        }
        mutex.unlock();
    };

    return ret;
}
