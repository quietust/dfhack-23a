bool empty() const { return x.empty(); }
unsigned size() const { return x.size(); }

void clear() {
    x.clear();
    y.clear();
    z.clear();
}

coord32 operator[] (unsigned idx) const {
    if (idx >= x.size())
        return coord32();
    else
        return coord32(x[idx], y[idx], z[idx]);
}

void erase(unsigned idx) {
    if (idx < x.size()) {
        x.erase(x.begin()+idx);
        y.erase(y.begin()+idx);
        z.erase(z.begin()+idx);
    }
}

void push_back(const coord &crd) {
    x.push_back(crd.x);
    y.push_back(crd.y);
    z.push_back(crd.z);
}
