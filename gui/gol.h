#pragma once

#include <algorithm>
#include <array>
#include <utility>

namespace gol
{

static constexpr size_t Width = 25;
static constexpr size_t Height = 25;

using Grid = std::array<bool, Width * Height>;

// Proper modulo because the standard % operator does not behave the way I
// want it to for negative numbers
static int Modulo(int modulus, int n)
{
    if (n < 0)
    {
        return modulus - ((-n) % modulus);
    }

    return n % modulus;
}

class Gol
{
  public:
    static size_t CountAliveNeighbours(Grid const &cells, int const x, int const y)
    {
        auto neighbours = size_t{0};

        static auto const directions =
            std::array{std::pair{-1, -1}, std::pair{0, -1}, std::pair{1, -1}, std::pair{1, 0},
                       std::pair{1, 1},   std::pair{0, 1},  std::pair{-1, 1}, std::pair{-1, 0}};

        for (auto const &[dx, dy] : directions)
        {
            auto const row = Modulo(Height, y + dy);
            auto const col = Modulo(Width, x + dx);
            if (cells.at(row * Width + col))
            {
                ++neighbours;
            }
        }

        return neighbours;
    }

    void Update()
    {
        auto newCells = Grid{};
        for (int y = 0; y < static_cast<int>(Height); ++y)
        {
            for (int x = 0; x < static_cast<int>(Width); ++x)
            {
                auto const aliveNeighbours = CountAliveNeighbours(cells, x, y);
                auto const index = y * Width + x;

                if (cells.at(index) && (2 == aliveNeighbours || 3 == aliveNeighbours))
                {
                    newCells.at(index) = true;
                }

                if (!cells.at(index) && 3 == aliveNeighbours)
                {
                    newCells.at(index) = true;
                }
            }
        }
        cells = newCells;
    }

    void Set(size_t const x, size_t const y, bool const alive)
    {
        if (x < Width && y < Height)
        {
            cells.at(y * Width + x) = alive;
        }
    }

    void Clear()
    {
        std::fill(cells.begin(), cells.end(), false);
    }

    Grid GetCells() const
    {
        return cells;
    }

  private:
    Grid cells;
};

} // namespace gol
