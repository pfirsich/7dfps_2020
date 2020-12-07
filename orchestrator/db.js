async function getGame(gameCode) {
  // return null;

  return {
    gameCode,
    host: "example.com",
    post: 1234,
  };
}

module.exports = {
  getGame,
};
