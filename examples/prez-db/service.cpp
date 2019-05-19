#include <cassert>
#include <cmath> // rand()
#include <iostream>
#include <sstream>

#include <net/http/request.hpp>
#include <net/http/response.hpp>
#include <net/interfaces>
#include <os>
#include <service>
#include <sl3/database.hpp>
#include <timers>

using namespace std::chrono;
using namespace sl3;

// instantiate a SQLite database
Database db(":memory:");
int      nextId = 1;

std::string
HTML_RESPONSE()
{
  const int color = rand();

  // Generate some HTML
  std::stringstream stream;

  stream << "[";

  Dataset ds = db.select(
      "SELECT title, points, author, timeToRead, comments FROM posts;");
  bool comma = false;
  for (auto &row : ds) {
    if (comma)
      stream << ",";
    else
      comma = true;

    stream << "{"
           << "     \"title\": \"" << row[0] << "\","
           << "     \"points\": " << row[1] << ","
           << "     \"author\": \"" << row[2] << "\","
           << "     \"timeToRead\": " << row[3] << ","
           << "     \"comments\": " << row[4] << ""
           << " }";
  }

  stream << "]";

  return stream.str();
}

http::Response
handle_request(const http::Request &req)
{
  printf("<Service> Request:\n%s\n", req.to_string().c_str());

  http::Response res;

  auto &header = res.header();

  header.set_field(http::header::Server, "IncludeOS/0.10");

  if (req.method() == http::GET && req.uri().to_string() == "/api/posts") {
    res.add_body(HTML_RESPONSE());

    header.set_field(http::header::Content_Type,
                     "application/json; charset=UTF-8");
    header.set_field(http::header::Content_Length,
                     std::to_string(res.body().size()));
  } else {
    res.set_status_code(http::Not_Found);
  }

  header.set_field(http::header::Connection, "close");

  auto cmd = db.prepare("INSERT INTO posts (id, title, points, author, "
                        "timeToRead, comments) VALUES (?,?,?,?,?,?);");
  cmd.execute(parameters(nextId++,
                         "Super Title",
                         rand() % 42,
                         "Mysterious Nobox",
                         rand() % 132,
                         rand() % 2000));

  return res;
}

void
Service::start()
{
  db.execute("CREATE TABLE posts(id INTEGER, title TEXT, points INTEGER, "
             "author TEXT, timeToRead INTEGER, comments INTEGER);");

  auto cmd = db.prepare("INSERT INTO posts (id, title, points, author, "
                        "timeToRead, comments) VALUES (?,?,?,?,?,?);");
  cmd.execute(
      parameters(nextId++, "Super Title", 42, "Mysterious Nobox", 63, 2));

  /**
   * REST server start
   **/

  auto &inet = net::Interfaces::get(0);

  // Print some useful netstats every 30 secs
  Timers::periodic(5s, 30s, [&inet](uint32_t) {
    printf("<Service> TCP STATUS:\n%s\n", inet.tcp().status().c_str());
  });

  // Set up a TCP server on port 80
  auto &server = inet.tcp().listen(80);

  // Add a TCP connection handler - here a hardcoded HTTP-service
  server.on_connect([](net::tcp::Connection_ptr conn) {
    printf("<Service> @on_connect: Connection %s successfully established.\n",
           conn->remote().to_string().c_str());

    // read async with a buffer size of 1024 bytes
    // define what to do when data is read
    conn->on_read(1024, [conn](auto buf) {
      printf("<Service> @on_read: %lu bytes received.\n", buf->size());
      try {
        const std::string data((const char *)buf->data(), buf->size());
        // try to parse the request
        http::Request req{data};

        // handle the request, getting a matching response
        auto res = handle_request(req);

        printf("<Service> Responding with %u %s.\n",
               res.status_code(),
               http::code_description(res.status_code()).data());

        conn->write(res);
      } catch (const std::exception &e) {
        printf("<Service> Unable to parse request:\n%s\n", e.what());
      }
    });
    conn->on_write([](size_t written) {
      printf("<Service> @on_write: %lu bytes written.\n", written);
    });
  });
}
