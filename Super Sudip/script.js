const canvas = document.getElementById("game");
const ctx = canvas.getContext("2d");
const scoreEl = document.getElementById("score");
const highScoreEl = document.getElementById("highScore");

const gridSize = 20;
const tileCount = canvas.width / gridSize;
const speedMs = 120;

let snake;
let dir;
let nextDir;
let food;
let score;
let running;
let gameLoopId;

const highScoreKey = "super_sudip_snake_high_score";
const highScore = Number(localStorage.getItem(highScoreKey) || 0);
highScoreEl.textContent = String(highScore);

function randomPos() {
  return {
    x: Math.floor(Math.random() * tileCount),
    y: Math.floor(Math.random() * tileCount),
  };
}

function placeFood() {
  let newFood = randomPos();
  while (snake.some((part) => part.x === newFood.x && part.y === newFood.y)) {
    newFood = randomPos();
  }
  food = newFood;
}

function resetGame() {
  snake = [{ x: 10, y: 10 }];
  dir = { x: 1, y: 0 };
  nextDir = { ...dir };
  score = 0;
  running = true;
  scoreEl.textContent = "0";
  placeFood();
}

function drawRect(x, y, color) {
  ctx.fillStyle = color;
  ctx.fillRect(x * gridSize, y * gridSize, gridSize - 1, gridSize - 1);
}

function draw() {
  ctx.fillStyle = "#08120d";
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  drawRect(food.x, food.y, "#ff6b6b");

  snake.forEach((part, i) => {
    drawRect(part.x, part.y, i === 0 ? "#95d5b2" : "#52b788");
  });

  if (!running) {
    ctx.fillStyle = "rgba(0,0,0,0.55)";
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    ctx.fillStyle = "#d8f3dc";
    ctx.font = "bold 28px Trebuchet MS";
    ctx.textAlign = "center";
    ctx.fillText("Game Over", canvas.width / 2, canvas.height / 2 - 8);
    ctx.font = "18px Trebuchet MS";
    ctx.fillText("Press Space to Restart", canvas.width / 2, canvas.height / 2 + 24);
  }
}

function tick() {
  if (!running) {
    draw();
    return;
  }

  dir = { ...nextDir };
  const head = { x: snake[0].x + dir.x, y: snake[0].y + dir.y };

  const outOfBounds =
    head.x < 0 || head.x >= tileCount || head.y < 0 || head.y >= tileCount;

  const hitSelf = snake.some((part) => part.x === head.x && part.y === head.y);

  if (outOfBounds || hitSelf) {
    running = false;
    draw();
    return;
  }

  snake.unshift(head);

  if (head.x === food.x && head.y === food.y) {
    score += 1;
    scoreEl.textContent = String(score);

    const currentHigh = Number(localStorage.getItem(highScoreKey) || 0);
    if (score > currentHigh) {
      localStorage.setItem(highScoreKey, String(score));
      highScoreEl.textContent = String(score);
    }

    placeFood();
  } else {
    snake.pop();
  }

  draw();
}

function setDirection(newX, newY) {
  if (newX === -dir.x && newY === -dir.y) return;
  nextDir = { x: newX, y: newY };
}

document.addEventListener("keydown", (event) => {
  const key = event.key.toLowerCase();

  if (key === " ") {
    if (!running) {
      resetGame();
      draw();
    }
    return;
  }

  if (key === "arrowup" || key === "w") setDirection(0, -1);
  else if (key === "arrowdown" || key === "s") setDirection(0, 1);
  else if (key === "arrowleft" || key === "a") setDirection(-1, 0);
  else if (key === "arrowright" || key === "d") setDirection(1, 0);
});

resetGame();
draw();
gameLoopId = setInterval(tick, speedMs);
