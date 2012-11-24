coord2d32(uint32_t _x, uint32_t _y) : x(_x), y(_y) {}
coord2d32(const coord2d &other) : x(other.x), y(other.y) {}
operator coord2d() const
{
    return coord2d(x, y);
}

bool isValid() const { return x != -30000; }
void clear() { x = y = -30000; }

bool operator==(const coord2d32 &other) const
{
    return (x == other.x) && (y == other.y);
}
bool operator!=(const coord2d32 &other) const
{
    return (x != other.x) || (y != other.y);
}

bool operator<(const coord2d32 &other) const
{
    if (x != other.x) return (x < other.x);
    return y < other.y;
}

coord2d32 operator+(const coord2d32 &other) const
{
    return coord2d32(x + other.x, y + other.y);
}
coord2d32 operator-(const coord2d32 &other) const
{
    return coord2d32(x - other.x, y - other.y);
}

coord2d32 operator/(int number) const
{
    return coord2d32((x < 0 ? x - number : x)/number, (y < 0 ? y - number : y)/number);
}
coord2d32 operator*(int number) const
{
    return coord2d32(x*number, y*number);
}
coord2d32 operator%(int number) const
{
    return coord2d32((x+number)%number, (y+number)%number);
}
coord2d32 operator&(int number) const
{
    return coord2d32(x&number, y&number);
}