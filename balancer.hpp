#pragma once
#define READQ_PER_CLIENT     4096
#define MAX_READQ_PER_NODE   8192
#define READQ_FOR_NODES      8192
#define MAX_OUTGOING_ATTEMPTS 100
// checking if nodes are dead or not
#define ACTIVE_INITIAL_PERIOD     8s
#define ACTIVE_CHECK_PERIOD      30s
// connection attempt timeouts
#define CONNECT_TIMEOUT          10s
#define CONNECT_THROW_PERIOD     20s
#define INITIAL_SESSION_TIMEOUT   5s
#define ROLLING_SESSION_TIMEOUT  60s

#include <net/inet4>
#include <liveupdate>
typedef net::Inet<net::IP4> netstack_t;
typedef net::tcp::Connection_ptr tcp_ptr;
typedef std::vector< std::pair<net::tcp::buffer_t, size_t> > queue_vector_t;
typedef delegate<void()> pool_signal_t;

struct Waiting {
  Waiting(tcp_ptr);
  Waiting(liu::Restore&, net::TCP&);
  void serialize(liu::Storage&);

  tcp_ptr conn;
  queue_vector_t readq;
  int total = 0;
};

struct Nodes;
struct Session {
  Session(Nodes&, int idx, bool talk, tcp_ptr inc, tcp_ptr out);
  bool is_alive() const;
  void handle_timeout();
  void timeout(Nodes&);
  void serialize(liu::Storage&);

  Nodes&    parent;
  const int self;
  int       timeout_timer;
  tcp_ptr   incoming;
  tcp_ptr   outgoing;
};

struct Node {
  Node(netstack_t& stk, net::Socket a, pool_signal_t sig);

  auto address() const noexcept { return this->addr; }
  int  connection_attempts() const noexcept { return this->connecting; }
  int  pool_size() const noexcept { return pool.size(); }
  bool is_active() const noexcept { return active; };

  void    restart_active_check();
  void    perform_active_check();
  void    stop_active_check();
  void    connect();
  tcp_ptr get_connection();

  netstack_t& stack;
private:
  net::Socket addr;
  pool_signal_t pool_signal;
  std::vector<tcp_ptr> pool;
  bool        active = false;
  int         active_timer = -1;
  signed int  connecting = 0;
};

struct Nodes {
  typedef std::deque<Node> nodevec_t;
  typedef nodevec_t::iterator iterator;
  typedef nodevec_t::const_iterator const_iterator;
  Nodes() {}

  size_t   size() const noexcept;
  const_iterator begin() const;
  const_iterator end() const;

  int32_t open_sessions() const;
  int64_t total_sessions() const;
  int32_t timed_out_sessions() const;
  int  pool_connecting() const;
  int  pool_size() const;

  template <typename... Args>
  void add_node(Args&&... args);
  void create_connections(int total);
  bool assign(tcp_ptr, queue_vector_t&);
  void create_session(bool talk, tcp_ptr inc, tcp_ptr out);
  void close_session(int, bool timeout = false);
  Session& get_session(int);

  void serialize(liu::Storage&);
  void deserialize(netstack_t& in, netstack_t& out, liu::Restore&);

private:
  nodevec_t nodes;
  int64_t   session_total = 0;
  int       session_cnt = 0;
  int       session_timeouts = 0;
  int       conn_iterator = 0;
  int       algo_iterator = 0;
  std::deque<Session> sessions;
  std::deque<int> free_sessions;
};

struct Balancer {
  Balancer(netstack_t& in, uint16_t port, netstack_t& out);

  int  wait_queue() const;
  int  connect_throws() const;

  void serialize(liu::Storage&, const liu::buffer_t*);
  void deserialize(liu::Restore&);

  Nodes nodes;

private:
  void incoming(tcp_ptr);
  void handle_connections();
  void handle_queue();
  std::vector<net::Socket> parse_node_confg();

  netstack_t& netin;
  netstack_t& netout;
  std::deque<Waiting> queue;
  int throw_retry_timer = -1;
  int throw_counter = 0;
};

template <typename... Args>
inline void Nodes::add_node(Args&&... args) {
  nodes.emplace_back(std::forward<Args> (args)...);
}
