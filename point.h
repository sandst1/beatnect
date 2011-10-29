// -*- mode: c++ -*-
#ifndef POINT_H
#define POINT_H

#include <QObject>

class Point: public QObject {
public:
  Point(int x = 0, int y = 0, int size = 0);
  ~Point();

  int getX() const;
  int getY() const;
  int getSize() const;

  void setX(int x);
  void setY(int y);
  void setSize(int size);


private:
  int m_x;
  int m_y;
  int m_size;
};


#endif // POINT_H
