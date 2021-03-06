#include <fstream>
#include <iostream>
#include <memory>

#include <grpcpp/grpcpp.h>

#include "app.grpc.pb.h"

using app::DataService;
using app::FileChunk;
using app::FileRequest;
using app::NumberReply;
using app::NumberRequest;
using app::StringReply;
using app::StringRequest;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;
using grpc::StatusCode;

static const size_t FILE_CHUNK_SIZE = 1 << 10;  // 1KB

class DataServiceImpl final : public DataService::Service
{
public:
  DataServiceImpl()
  {
    m_numbers = {{"one", 1}, {"two", 2}, {"three", 3}, {"four", 4}};
    m_strings = {"foo", "bar", "spam", "ham", "eggs"};
  }

private:
  // Obtains the number with the given name. The available names are stored in
  // m_numbers: [ "one", "two", "three", "four" ].
  //
  // If the given name doesn't name any known number a NOT FOUND Status Code
  // is returned.
  Status GetNumber(ServerContext* context, const NumberRequest* request,
                   NumberReply* reply) override
  {
    (void)(context);
    const std::string name = request->name();
    const auto iter = m_numbers.find(name);
    if (iter == m_numbers.cend())
    {
      const std::string details("Number with name " + name + " not found");
      return Status(StatusCode::NOT_FOUND, details);
    }
    reply->set_number(iter->second);
    return Status::OK;
  }

  // Obtains the string at the given index in the storage vector. The string
  // storage vector is m_strings: [ "foo", "bar", "spam", "ham", "eggs" ].
  //
  // If the given index is out of the bounds of m_strings a NOT FOUND Status
  // Code is returned.
  Status GetString(ServerContext* context, const StringRequest* request,
                   StringReply* reply) override
  {
    (void)(context);
    const size_t index = request->index();
    if (index < 0 || index > (m_strings.size() - 1))
    {
      const std::string details("String with index " + std::to_string(index) +
                                " not found");
      return Status(StatusCode::NOT_FOUND, details);
    }
    reply->set_string_(m_strings[index]);
    return Status::OK;
  }

  // Returns a file as a stream of chunks. This way, both the server and the
  // client do not need to bring the whole file to memory. This approach is
  // particularly appropriate for trasmitting big files (> 1GB). Also, this
  // could facilitate to handle interruptions or partial file recoveries in the
  // future.
  //
  // If the given filename is not found in the filesystem a NOT FOUND Status
  // Code is returned.
  Status GetFile(ServerContext* context, const FileRequest* request,
                 ServerWriter<FileChunk>* writer) override
  {
    (void)(context);
    std::ifstream ifs;
    std::string filename = request->filename();
    ifs.open(filename, std::ifstream::in | std::ifstream::binary);
    if (!ifs)
    {
      const std::string details("File " + filename + " not found");
      return Status(StatusCode::NOT_FOUND, details);
    }
    std::string chunk(FILE_CHUNK_SIZE, '\0');
    while (ifs)
    {
      ifs.read(&chunk[0], FILE_CHUNK_SIZE);
      FileChunk fileChunk;
      fileChunk.set_content(chunk);
      fileChunk.set_size(ifs.gcount());
      writer->Write(fileChunk);
    }
    return Status::OK;
  }

private:
  std::map<std::string, int> m_numbers;
  std::vector<std::string> m_strings;
};

void RunServer(const std::string& hostname, const std::string& port)
{
  std::string server_address(hostname + ":" + port);
  DataServiceImpl service;
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char* args[])
{
  if (argc < 3)
  {
    std::cout << "Usage: ./server <hostname> <port>" << std::endl;
    std::cout << "EXAMPLE" << std::endl;
    std::cout << "    ./server localhost 50051" << std::endl;
    return 1;
  }
  const std::string hostname(args[1]);
  const std::string port(args[2]);
  RunServer(hostname, port);
  return 0;
}
