#include "point.h"

Point::Point(int x, int y, int size) :
  m_x(x), m_y(y), m_size(size)
{
  // nothing
}

Point::~Point()
{
  // nothing
}


int
Point::getX() const
{
  return m_x;
}

int
Point::getY() const
{
  return m_y;
}

int
Point::getSize() const
{
  return m_size;
}


void
Point::setX(int x)
{
  m_x = x;
}

void
Point::setY(int y)
{
  m_y = y;
}

void
Point::setSize(int size)
{
  m_size = size;
}

