#pragma once

template <typename Request, typename Response> class CorsMiddleware {
public:
  static bool process(Request &req, Response &resp) {
    resp.setHeader("Access-Control-Allow-Origin", "*");
    resp.setHeader("Access-Control-Allow-Methods",
                   "GET, POST, PUT, DELETE, OPTIONS, PATCH");
    resp.setHeader(
        "Access-Control-Allow-Headers",
        "Content-Type, Authorization, X-Requested-With, Accept, Origin");
    resp.setHeader("Access-Control-Allow-Credentials", "true");
    resp.setHeader("Access-Control-Max-Age", "86400");
    resp.setHeader("Access-Control-Expose-Headers",
                   "Content-Length, Content-Range, X-Total-Count");
    if (req.getMethod() == "OPTIONS") {
      resp.setStatus(200);
      resp.setBodyContent("");
      return false;
    }
    return true;
  }
};
