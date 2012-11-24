coord32(uint32_t _x, uint32_t _y, uint32_t _z) : x(_x), y(_y), z(_z) {}

coord32(const coord &other) : x(other.x), y(other.y), z(other.z) {}
operator coord() const
{
    return coord(x, y, z);
}

bool isValid() const { return x != -30000; }
void clear() { x = y = z = -30000; }

bool operator==(const coord32 &other) const
{
    return (x == other.x) && (y == other.y) && (z == other.z);
}
bool operator!=(const coord32 &other) const
{
    return (x != other.x) || (y != other.y) || (z != other.z);
}

bool operator<(const coord32 &other) const
{
    if (x != other.x) return (x < other.x);
    if (y != other.y) return (y < other.y);
    return z < other.z;
}

coord32 operator+(const coord32 &other) const
{
    return coord32(x + other.x, y + other.y, z + other.z);
}
coord32 operator-(const coord32 &other) const
{
    return coord32(x - other.x, y - other.y, z - other.z);
}

coord32 operator/(int number) const
{
    return coord32((x < 0 ? x - number : x)/number, (y < 0 ? y - number : y)/number, z);
}
coord32 operator*(int number) const
{
    return coord32(x*number, y*number, z);
}
coord32 operator%(int number) const
{
    return coord32((x+number)%number, (y+number)%number, z);
}

coord32 operator-(int number) const
{
    return coord32(x,y,z-number);
}
coord32 operator+(int number) const
{
    return coord32(x,y,z+number);
}
