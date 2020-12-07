async function startGame() {
  const gameCode = Math.round(Math.random() * 2 ** 31)
    .toString(16)
    .toUpperCase();

  return {
    gameCode,
    host: "example.com",
    post: 1234,
  };
}

module.exports = {
  startGame,
};
