template <typename ScalarT>
class Position;

using Pos = Position<float>;

template <typename ScalarT>
class Position
{
public:
  // Cartesian coordinates
  void setCartesian (ScalarT const &x, ScalarT const &y, ScalarT const &z);
  void setX (ScalarT const &x);
  void setY (ScalarT const &y);
  void setZ (ScalarT const &z);

  ScalarT x () const;
  ScalarT y () const;
  ScalarT z () const;

  // Spherical coordinates
  void setSpherical (ScalarT const &azimuth, ScalarT const &elevation,
                     ScalarT const &radius);
  void setAzimuth (ScalarT const &x);
  void setElevation (ScalarT const &y);
  void setRadius (ScalarT const &z);

  ScalarT azimuth () const;
  ScalarT elevation () const;
  ScalarT radius () const;

private:
  ScalarT x_, y_, z_;
};

template <typename ScalarT>
void
Position<ScalarT>::setCartesian (ScalarT const &x, ScalarT const &y,
                                 ScalarT const &z)
{
  x_ = x;
  y_ = y;
  z_ = z;
}
