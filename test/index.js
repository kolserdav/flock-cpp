const { lock, unlock, isLocked } = require("../index");

const FILE_PATH = "tmp/das1";

const interval = setInterval(() => {
  console.log("Wait to lock file from other process...");
}, 1000);

(async () => {
  console.log(`File ${FILE_PATH} is locked: `, await isLocked(FILE_PATH));
  const res = await lock(FILE_PATH).catch((e) => {
    console.error("Failed to lock file", e);
  });
  if (res === undefined) {
    return;
  }
  console.log("File locked", res);
  const timeout = 5000;
  console.log(`Waithing ${timeout / 1000} seconds to unlock`);
  setTimeout(async () => {
    await unlock(res).catch((e) => {
      console.error("Failed to unlock", e);
    });
    console.log("File unlocked");
    clearInterval(interval);
  }, timeout);
})();
