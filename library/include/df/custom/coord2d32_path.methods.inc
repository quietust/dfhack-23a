unsigned size() const { return x.size(); }

coord2d32 operator[] (unsigned idx) const {
    if (idx >= x.size())
        return coord2d32();
    else
        return coord2d32(x[idx], y[idx]);
}

void erase(unsigned idx) {
    if (idx < x.size()) {
        x.erase(x.begin()+idx);
        y.erase(y.begin()+idx);
    }
}

void push_back(const coord2d32 &crd) {
    x.push_back(crd.x);
    y.push_back(crd.y);
}
