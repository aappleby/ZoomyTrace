console.log("WebUSB test server starting at ", Date.now());

//ajksdlfksj

flerp = <div>alskdfjlskjdf</div>;

const BASE_PATH = "./";
Bun.serve({
  port: 3000,
  async fetch(req) {
    //const filePath = BASE_PATH + new URL(req.url).pathname;
    //const file = Bun.file(filePath);
    //return new Response(file);
    //return new Response("Hello Spicy sjdkfjs Worldsldsdfsffjsl");
    return new Response("flarp");
  },
  error() {
    return new Response(null, { status: 404 });
  },
});
