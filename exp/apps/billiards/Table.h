/** Billiards Table  -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2011, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @section Description
 *
 * Billiards Table.
 *
 * @author <ahassaan@ices.utexas.edu>
 */

#ifndef _TABLE_H_
#define _TABLE_H_

#include <vector>

#include <cstdlib>
#include <cstdio>
#include <ctime>

#include "FPutils.h"
#include "GeomUtils.h"
#include "Cushion.h"
#include "Ball.h"
#include "Collision.h"
#include "Event.h"

#include "Galois/Runtime/ll/gio.h"

class Table {

public:
  struct DefaultValues {
    static const FP BALL_MASS;
    static const FP BALL_RADIUS;
    static const FP MIN_SPEED;
    static const FP MAX_SPEED;
  };


protected:
  unsigned numBalls;
  FP length;
  FP width;
  std::vector<Galois::gstl::Vector<Event> > eventsPerBall;
  

  std::vector<Cushion*> cushions;
  std::vector<Ball*> balls;


  Table& operator = (const Table& that) { abort (); return *this; }

public:
  Table (unsigned numBalls, const FP& length, const FP& width) 
    : numBalls (numBalls), length (length), width (width), eventsPerBall (numBalls) {

    srand (0); // TODO: use time (nullptr) later 
    createCushions ();
    createBalls ();
  }

  template <typename I>
  Table (const I ballsBeg, const I ballsEnd, const FP& length, const FP& width)
    : numBalls (std::distance (ballsBeg, ballsEnd)), length (length), width (width), eventsPerBall (numBalls)
  {
    srand (0);
    createCushions ();

    for (I i = ballsBeg; i != ballsEnd; ++i) {
      balls.push_back (new Ball (*i));
    }
  }

  Table (const Table& that) 
    : numBalls (that.numBalls), length (that.length), width (that.width), eventsPerBall (that.eventsPerBall) {

      copyVecPtr (that.cushions, this->cushions);
      copyVecPtr (that.balls, this->balls);
  }

  ~Table () {
    freeVecPtr (cushions);
    freeVecPtr (balls);
  }

  unsigned getNumBalls () const { return numBalls; }

  const FP& getLength () const { return length; }

  const FP& getWidth () const { return width; }

  Cushion* getCushion (RectSide side) const { 
    assert (int(side) < int (cushions.size ()));
    return cushions [int (side)];
  }

  const Ball& getBallByID (unsigned id) const {
    assert (id < balls.size ());

    // XXX: assumes ball ids and indices are same
    assert (balls[id] != nullptr);
    return *balls[id]; 
  }

  void logCollisionEvent (const Event& e) {

   

    if (e.notStale () && (e.getKind () == Event::BALL_COLLISION || e.getKind () == Event::CUSHION_COLLISION)) {
      
      assert (eventsPerBall.size () == getNumBalls ());

      eventsPerBall [e.getBall ()->getID ()].push_back (e);

      if (e.getKind () == Event::BALL_COLLISION) {
        eventsPerBall [e.getOtherBall ()->getID ()].push_back (e);
      }
    }
  }

  void printEventLogs (void) {

    for (size_t i = 0; i < eventsPerBall.size (); ++i) {

      std::printf ("===== Events for Ball %zd =======\n", i);

      for (const Event& e: eventsPerBall [i]) {
        std::cout << e.str () << std::endl;
      }
    }
  }

  void diffEventLogs (const Table& that, const char* const thatName) {

    for (size_t i = 0; i < numBalls; ++i) {
      std::printf ("===== Events for Ball %zd =======\n", i);

      if (this->eventsPerBall[i].size () != that.eventsPerBall[i].size ()) {
        std::printf ("Number of events for Ball %zd differ with %s\n", i, thatName);
      }

      for (size_t j = 0; j < std::max (this->eventsPerBall[i].size (), that.eventsPerBall[i].size ()); ++j) {

        const Event& e1 = this->eventsPerBall[i][j];
        const Event& e2 = that.eventsPerBall[i][j];

        if (j >= this->eventsPerBall[i].size ()) {
          std::printf ("Only %s has event %s\n",  thatName, e2.str ().c_str ());
          continue;
        }

        if (j >= that.eventsPerBall[i].size ()) {
          std::printf ("%s does not have event %s\n",  thatName, e1.str ().c_str ());
          continue;
        }

        if (e1 != e2) {
          std::printf ("Differing events: this has %s\n %s has %s\n", e1.str ().c_str (), thatName, e2.str ().c_str ());
        }
        
      }

    }
  }

  void genInitialEvents (std::vector<Event>& initEvents, const FP& endtime) {

    initEvents.clear ();

    for (std::vector<Ball*>::iterator i = balls.begin (), ei = balls.end ();
        i != ei; ++i) {

      addNextEventIntern (initEvents, *i, endtime); 
    }
  }

  void advance (const FP& simTime) {
    for (Ball* b: balls) {
      if (simTime > b->time ()) {
        b->update (b->vel (), simTime);

      } else {
        assert (b->time () == simTime);
      }
    }
  }

  void check () const {

    for (size_t i = 0; i < balls.size (); ++i) {
      const Ball* b0 = balls[i];
      FP px = b0->pos ().getX ();
      FP py = b0->pos ().getY ();

      // TODO: boundary tests don't include radius. 
      if (px < FP (0.0) || px > length) {
        std::cerr << "!!!  ERROR: Ball out of X lim: " << b0->str () << std::endl;
      }

      if (py < FP (0.0) || py > width) {
        std::cerr << "!!!  ERROR: Ball out of Y lim: " << b0->str () << std::endl;
      }

      // check for overlap
      for (size_t j = i + 1; j < balls.size (); ++j) {
        const Ball* b1 = balls[j];

        FP d = b0->pos ().dist (b1->pos ());

        if (d < (b0->radius () + b1->radius ())) {
          std::cerr << "!!!  ERROR: Balls overlap, distance: " << d << ",   ";
          std::cerr << b0->str () << "    ";
          std::cerr << b1->str () << std::endl;
        }

      }

    }

  }

  //! @param addList
  //! @param b1, ball involved in a collision just simualtedrevious collision 
  //! @param b2, other object involved in a collision just simualted
  //!
  //! The idea is to avoid generating a collision event with previous object. Because
  //! the collision physics is going to generate a new collision with the same object the
  //! ball just collided with, but we have already simulated a collision and ball should be moving
  //! in another direction and pick a different collision

  template <typename C>
  void addNextEvents (const Event& e, C& addList, const FP& endtime) const {

    switch (e.getKind ()) {
      case Event::BALL_COLLISION:
        addEventsForBallColl (e, addList, endtime);
        break;

      case Event::CUSHION_COLLISION:
        addEventsForCushColl (e, addList, endtime);
        break;

      default:
        GALOIS_DIE ("unkown event kind");
    }
  }

  FP sumEnergy () const {

    FP sumKE = 0.0;

    for (std::vector<Ball*>::const_iterator i = balls.begin (), ei = balls.end ();
        i != ei; ++i) {

      sumKE += (*i)->ke ();
    }

    return sumKE;
  }

  template <bool printDiff>
  bool cmpState (const Table& that) const {

    bool equal = true;

    if (numBalls != that.numBalls) {
      equal = false;
      if (printDiff) {
        fprintf (stderr, "Different number of balls, this.numBalls=%d, that.numBalls=%d\n" 
       , this->numBalls, that.numBalls);
      }
    }


    for (size_t i = 0; i < numBalls; ++i) {
      const Ball& b1 = *(this->balls[i]);
      const Ball& b2 = *(that.balls[i]);
      
      if (b1.getID () != b2.getID ()) {
        std::cerr << "Balls with different ID at same index i" << std::endl;
        abort ();
      }

      if (b1.mass () != b2.mass ()) {
        std::cerr << "Balls with same ID and different masses" << std::endl;
        abort ();
      }

      if (b1.radius () != b2.radius ()) {
        std::cerr << "Balls with same ID and different radii" << std::endl;
        abort ();
      }

      // checkError<false> does not call assert
      if (!FPutils::checkError (b1.pos (), b2.pos (), false)) {

        equal = false;

        if (printDiff) {
          printBallAttr ("pos", i, b1.pos (), b2.pos ());
        }
      }

      if (!FPutils::checkError (b1.vel (), b2.vel (), false)) {

        equal = false;

        if (printDiff) {
          printBallAttr ("vel", i, b1.vel (), b2.vel ());
        }
      }

      if (!FPutils::checkError (b1.time (), b2.time (), false)) {
        equal = false;
      
        if (printDiff) {
          printBallAttr ("time", i, b1.time (), b2.time ());
        }
      }

      // Commenting out comparison of collision counter so that
      // sectored and flat simulations can be compared

      // if (b1.collCounter () != b2.collCounter ()) {
        // equal = false;
      // 
        // if (printDiff) {
          // printBallAttr ("collCounter", i, b1.collCounter (), b2.collCounter ());
        // }
      // }

      if (!printDiff && !equal) {
        break;
      }


    }


    return equal;
  }



  template <typename StrmTy>
  StrmTy& printState (StrmTy& out) {

    // out << "Table setup, length=" << length << ", width=" << width
      // << ", number of balls=" << numBalls << std::endl;

    // out << "Cushions = ";
    // printVecPtr (out, cushions);

    out << "Balls = ";
    printVecPtr (out, balls);

    return out;

  }

  void writeConfig (const char* const confName="config.csv") const {

    FILE* confFile = fopen (confName, "w");
    assert (confFile != nullptr);

    fprintf (confFile, "length, width, num_balls, ball.mass, ball.radius\n");
    fprintf (confFile, "%e, %e, %d, %e, %e\n",
        double (getLength ()), 
        double (getWidth ()), 
        getNumBalls (), 
        double (DefaultValues::BALL_MASS), 
        double (DefaultValues::BALL_RADIUS));

    fclose (confFile);

  }

  void ballsToCSV (const char* ballsFile = "balls.csv") const {
    FILE* ballsFH = fopen (ballsFile, "w");

    if( ballsFH == nullptr) { abort (); }

    // fprintf (ballsFH, "ball_id, mass, radius, pos_x, pos_y, vel_x, vel_y\n");
    fprintf (ballsFH, "ball.id, ball.pos.x, ball.pos.y, ball.vel.x, ball.vel.y\n");

    for (std::vector<Ball*>::const_iterator i = balls.begin ()
        , ei = balls.end (); i != ei; ++i) {
      const Ball& b = *(*i);
      
//       fprintf (ballsFH, "%d, %g, %g, %g, %g, %g, %g\n"
  //         , b.getID (), b.mass (), b.radius (), b.pos ().getX (), b.pos ().getY (), b.vel ().getX (), b.vel ().getY ());
      fprintf (ballsFH, "%d, %e, %e, %e, %e\n", 
          b.getID (), 
          double (b.pos ().getX ()), 
          double (b.pos ().getY ()), 
          double (b.vel ().getX ()), 
          double (b.vel ().getY ()));
    }
    
    fclose (ballsFH);
  }

  void cushionsToCSV (const char* cushionsFile="cushions.csv") const {

    FILE* cushionsFH = fopen (cushionsFile, "w");

    if (cushionsFH == nullptr) { abort (); }

    fprintf (cushionsFH, "cushion_id, start_x, start_y, end_x, end_y\n");

    for (std::vector<Cushion*>::const_iterator i = cushions.begin ()
        , ei = cushions.end (); i != ei; ++i) {
      const Cushion& c = *(*i);
      const LineSegment& l = c.getLineSegment ();

      fprintf (cushionsFH, "%d, %g, %g, %g, %g"
          , c.getID (), 
          double (l.getBegin ().getX ()), 
          double (l.getBegin ().getY ()), 
          double (l.getEnd ().getX ()), 
          double (l.getEnd ().getY ()));
    }
  }

protected:

  void createCushions () {
    // create all cushions by specifying endpoints clockwise
    // in a 2D plane whose bottom left corner is at origin
    //
    // First vectors for each corner
    Vec2 bottomLeft (0.0, 0.0);
    Vec2 topLeft (0.0, width);
    Vec2 topRight (length, width);
    Vec2 bottomRight (length, 0.0);

    // adding clockwise so that we can index them with RectSide
    // left
    Cushion* left =  new Cushion (0, bottomLeft, topLeft);
    cushions.push_back (left);
    // top
    Cushion* top =  new Cushion (1, topLeft, topRight);
    cushions.push_back (top);
    // right
    Cushion* right =  new Cushion (2, topRight, bottomRight);
    cushions.push_back (right);
    // bottom
    Cushion* bottom =  new Cushion (3, bottomRight, bottomLeft);
    cushions.push_back (bottom);
  }


  void createBalls () {
    // TODO: make sure the balls are not placed overlapping
    // i.e. make sure that mag (b1.pos - b2.pos) > (b1.radius + b2.radius)

    for (unsigned i = 0; i < numBalls; ++i) {


      // fixed radius for now
      FP radius = DefaultValues::BALL_RADIUS;

      Vec2 pos = genBallPos(i, radius);

      // assign random initial velocity
      FP v_x = genRand ((FP (0.0) - DefaultValues::MAX_SPEED), DefaultValues::MAX_SPEED);
      FP v_y = genRand ((FP (0.0) - DefaultValues::MAX_SPEED), DefaultValues::MAX_SPEED);

      Vec2 vel (v_x, v_y);

      assert (vel.mag () < (FP (2.0) * DefaultValues::MAX_SPEED));

      Ball* b = new Ball (i, pos, vel, DefaultValues::BALL_MASS, radius);

      balls.push_back (b);
    }
  }


  Vec2 genBallPos (size_t numGenerated, const FP& radius) {

    Vec2 pos (FP (-1.0), FP (-1.0));

    unsigned numAttempts = 0;;
    const unsigned MAX_ATTEMPTS = 10;

    bool good = false;

    while (!good) {

      if (numAttempts >= MAX_ATTEMPTS) {
        std::cerr << "Could not find a place to put newly generated ball" << std::endl;
        abort ();
      }

      // compute ball's position
      // We want to place the ball such that
      // its center is at least radius away from each cushion
       
      FP x_pos = genRand (radius, length - radius);
      FP y_pos = genRand (radius, width - radius);

      pos = Vec2(x_pos, y_pos);

      // And we also want to make sure that distance between above pos and
      // already generated balls is > (b1.radius + b2.radius)
      good = true;
      for (unsigned j = 0; j < numGenerated; ++j) {
        if ((balls[j]->pos () - pos).mag () <= (radius + balls[j]->radius ())) {
          good = false;
          break;
        }
      }

    }

    return pos;

  }


  FP genRand (const FP& lim_min, const FP& lim_max) {
    // FP r = FP (rand ()) / FP (RAND_MAX);
    FP r = double (rand () % 1024) / 1024.0;
    FP ret = lim_min + r*(lim_max - lim_min);
    
    return FPutils::truncate (ret);
  }


  template <typename C>
  void addEventsForBallColl (const Event& e, C& addList, const FP& endtime) const {

    assert (e.getKind () == Event::BALL_COLLISION);

    const Ball* b1 = e.getBall ();
    const Ball* b2 = e.getOtherBall ();
    
    // firstBallChanged or otherBallChanged should return true 
    // for an invalid event 'e'

    if (!e.firstBallChanged ()) {

      addNextEventIntern  (addList, b1, endtime, &e);
    }

    if (!e.otherBallChanged ()) {
      // b2 has not collided with anything yet
      addNextEventIntern (addList, b2, endtime, &e);

    }
  }

  template <typename C>
  void addEventsForCushColl (const Event& e, C& addList, const FP& endtime) const {

    assert (e.getKind () == Event::CUSHION_COLLISION);
    if (!e.firstBallChanged ()) {
      addNextEventIntern (addList, e.getBall (), endtime, &e);
    }
  }

  template <typename C>
  void addNextEventIntern (C& addList, const Ball* ball, const FP& endtime, const Event* prevEvent=nullptr) const {

    assert (ball != nullptr);

    Galois::optional<Event> ballColl = Collision::computeNextEvent (Event::BALL_COLLISION, ball, this->balls.begin (), this->balls.end (), endtime);
    Galois::optional<Event> cushColl = Collision::computeNextEvent (Event::CUSHION_COLLISION, ball, this->cushions.begin (), this->cushions.end (), endtime);



    if (ballColl && prevEvent && prevEvent->notStale ()) {
      assert (*ballColl != *prevEvent);
    }


    if (cushColl && prevEvent && prevEvent->notStale ()) {
      assert (*cushColl != *prevEvent);
    }


    if (ballColl && cushColl) {
      addList.push_back (std::min (*ballColl, *cushColl));

    } else if (ballColl) {
      addList.push_back (*ballColl);

    } else if (cushColl) {
      addList.push_back (*cushColl);

    } else {
      assert (!ballColl && !cushColl);
    }


  }


  // template <typename C>
  // void addNextEventIntern (C& addList, const Ball* ball, const Ball* prevBall, const Cushion* prevCushion, const FP& endtime) const {
// 
    // assert (ball != nullptr);
// 
    // std::pair<const Ball*, FP> ballColl = Collision::computeNextCollision (ball, this->balls.begin (), this->balls.end (), prevBall, endtime);
    // std::pair<const Cushion*, FP> cushColl = Collision::computeNextCollision (ball, this->cushions.begin (), this->cushions.end (), prevCushion, endtime);
// 
    // if (ballColl.first != nullptr && cushColl.first != nullptr) {
// 
      // // giving preference to cushion collision when a ball is hitting another ball
      // // and a cushion at the same time
// 
      // if (FPutils::almostEqual (ballColl.second, cushColl.second)) {
// 
        // addList.push_back (Event::makeCushionCollision (ball, cushColl.first, cushColl.second));
// 
      // } else {
// 
        // const Event& e1 = Event::makeCushionCollision (ball, cushColl.first, cushColl.second);
// 
        // const Event& e2 = Event::makeBallCollision (ball, ballColl.first, ballColl.second);
// 
// 
        // addList.push_back (std::min (e1, e2));
      // }
// 
// 
    // } else if (ballColl.first != nullptr) {
// 
      // addList.push_back (Event::makeBallCollision (ball, ballColl.first, ballColl.second));
// 
    // } else if (cushColl.first != nullptr) {
// 
      // addList.push_back (Event::makeCushionCollision (ball, cushColl.first, cushColl.second));
// 
    // } else {
// 
      // assert ((ballColl.second < 0.0 || ballColl.second > endtime) 
          // && "Valid collision time but no valid collision ball");
// 
      // assert ((cushColl.second < 0.0 || cushColl.second > endtime) 
          // && "Valid collision time but no valid collision cushion");
    // }
// 
  // }


  template <typename T>
  static void copyVecPtr (const std::vector<T*>& src, std::vector<T*>& dst) {
    dst.resize (src.size (), nullptr);
    for (size_t i = 0; i < src.size (); ++i) {
      dst[i] = new T(*(src[i]));
    }
  }

  template <typename T>
  static void freeVecPtr (std::vector<T*>& vec) {
    for (typename std::vector<T*>::iterator i = vec.begin (), ei = vec.end ();
        i != ei; ++i) {

      delete *i;
      *i = nullptr;
    }
  }

  template <typename StrmTy, typename T>
  static StrmTy& printVecPtr (StrmTy& out, std::vector<T*>& vec) {

    out << "[" << std::endl;
    for (typename std::vector<T*>::const_iterator i = vec.begin (), ei = vec.end ();
        i != ei; ++i) {

      out << (*i)->str () << ", " << std::endl;
    }
    out << "]"  << std::endl;
    
    return out;
  }

  template <typename T>
  static void printBallAttr (const char* attrName, size_t index, const T& b1attr, const T& b2attr, std::ostream& out=std::cerr) {

    out << "{";
    printBallAttrHelper ("this", attrName, index, b1attr, out);
    out << "} != {";
    printBallAttrHelper ("that", attrName, index, b2attr, out);
    out << "}" << std::endl;

  }


  template <typename T>
  static void printBallAttrHelper (const char* refName, const char* attrName, size_t index, const T& attrVal, std::ostream& out) {
    out << refName << ".balls[" << index << "]." << attrName << " () == " << attrVal; 
  }
};


#endif //  _TABLE_H_
