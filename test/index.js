const { lock } = require("../index");

(async () => {
  const res = await lock("tmp/das1").catch((e) => {
    console.error("Failed to say", e);
  });
  console.log(1, res);
})();

setInterval(() => {
  console.log("Async");
}, 1000);
