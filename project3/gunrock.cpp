#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <deque>

#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HttpService.h"
#include "HttpUtils.h"
#include "FileService.h"
#include "MySocket.h"
#include "MyServerSocket.h"
#include "dthread.h"

using namespace std;

int PORT = 8080;
int THREAD_POOL_SIZE = 1;
size_t BUFFER_SIZE = 1;
string BASEDIR = "static";
string SCHEDALG = "FIFO";
string LOGFILE = "/dev/null";

vector<HttpService *> services;

// Connection queue and synchronization constructs
std::deque<MySocket *> connection_queue;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;

HttpService *find_service(HTTPRequest *request)
{
  // find a service that is registered for this path prefix
  for (unsigned int idx = 0; idx < services.size(); idx++)
  {
    if (request->getPath().find(services[idx]->pathPrefix()) == 0)
    {
      return services[idx];
    }
  }

  return NULL;
}

void invoke_service_method(HttpService *service, HTTPRequest *request, HTTPResponse *response)
{
  stringstream payload;

  // invoke the service if we found one
  if (service == NULL)
  {
    // not found status
    response->setStatus(404);
  }
  else if (request->isHead())
  {
    service->head(request, response);
  }
  else if (request->isGet())
  {
    service->get(request, response);
  }
  else
  {
    // The server doesn't know about this method
    response->setStatus(501);
  }
}

void handle_request(MySocket *client)
{
  HTTPRequest *request = new HTTPRequest(client, PORT);
  HTTPResponse *response = new HTTPResponse();
  stringstream payload;

  // read in the request
  bool readResult = false;
  try
  {
    payload << "client: " << (void *)client;
    sync_print("read_request_enter", payload.str());
    readResult = request->readRequest();
    sync_print("read_request_return", payload.str());
  }
  catch (...)
  {
    // swallow it
  }

  if (!readResult)
  {
    // there was a problem reading in the request, bail
    delete response;
    delete request;
    sync_print("read_request_error", payload.str());
    return;
  }

  HttpService *service = find_service(request);
  invoke_service_method(service, request, response);

  // send data back to the client and clean up
  payload.str("");
  payload.clear();
  payload << " RESPONSE " << response->getStatus() << " client: " << (void *)client;
  sync_print("write_response", payload.str());
  cout << payload.str() << endl;
  client->write(response->response());

  delete response;
  delete request;

  payload.str("");
  payload.clear();
  payload << " client: " << (void *)client;
  sync_print("close_connection", payload.str());
  client->close();
  delete client;
}

// Worker thread function
void *worker_thread_func(void *arg)
{
  while (true)
  {
    dthread_mutex_lock(&queue_mutex);

    // Wait for available connections
    while (connection_queue.empty())
    {
      dthread_cond_wait(&queue_not_empty, &queue_mutex);
    }

    // Retrieve connection
    MySocket *client = connection_queue.front();
    connection_queue.pop_front();

    // Signal main thread if space is available in the queue
    dthread_cond_signal(&queue_not_full);

    dthread_mutex_unlock(&queue_mutex);

    if (client != nullptr)
    {
      handle_request(client);
    }
  }
  return nullptr;
}

int main(int argc, char *argv[])
{

  signal(SIGPIPE, SIG_IGN);
  int option;

  while ((option = getopt(argc, argv, "d:p:t:b:s:l:")) != -1)
  {
    switch (option)
    {
    case 'd':
      BASEDIR = string(optarg);
      break;
    case 'p':
      PORT = atoi(optarg);
      break;
    case 't':
      THREAD_POOL_SIZE = atoi(optarg);
      break;
    case 'b':
      BUFFER_SIZE = atoi(optarg);
      break;
    case 's':
      SCHEDALG = string(optarg);
      break;
    case 'l':
      LOGFILE = string(optarg);
      break;
    default:
      cerr << "usage: " << argv[0] << " [-p port] [-t threads] [-b buffers]" << endl;
      exit(1);
    }
  }

  set_log_file(LOGFILE);

  sync_print("init", "");
  MyServerSocket *server = new MyServerSocket(PORT);
  MySocket *client;

  // The order that you push services dictates the search order
  // for path prefix matching
  services.push_back(new FileService(BASEDIR));

  // while (true)
  // {
  //   sync_print("waiting_to_accept", "");
  //   client = server->accept();
  //   sync_print("client_accepted", "");
  //   handle_request(client);
  // }

  // Create a thread pool
  for (int i = 0; i < THREAD_POOL_SIZE; ++i)
  {
    pthread_t thread;
    dthread_create(&thread, nullptr, worker_thread_func, nullptr);
    dthread_detach(thread); // Detach so we don’t need to manage thread join
  }

  while (true)
  {
    sync_print("waiting_to_accept", "");
    client = server->accept();
    sync_print("client_accepted", "");

    dthread_mutex_lock(&queue_mutex);

    // Wait if buffer is full
    while (connection_queue.size() >= BUFFER_SIZE)
    {
      dthread_cond_wait(&queue_not_full, &queue_mutex);
    }

    connection_queue.push_back(client);

    // Signal worker threads that a connection is available
    dthread_cond_signal(&queue_not_empty);

    dthread_mutex_unlock(&queue_mutex);
  }
}
