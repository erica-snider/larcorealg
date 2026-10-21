/**
 * @file   GeometryIteratorLoopTestAlg.cxx
 * @brief  Tests the correct iteration of the geo::Geometry iterators
 * @author Gianluca Petrillo (petrillo@fnal.gov)
 * @date   August 25, 2014
 */

// our header
#include "GeometryIteratorLoopTestAlg.h"

// LArSoft includes
#include "larcorealg/Geometry/CryostatGeo.h"
#include "larcorealg/Geometry/GeometryCore.h"
#include "larcorealg/Geometry/PlaneGeo.h"
#include "larcorealg/Geometry/TPCGeo.h"
#include "larcorealg/Geometry/WireGeo.h"
#include "larcorealg/Geometry/WireReadoutGeom.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"
#include "test/Geometry/IteratorTypes.h"

// Framework includes
#include "cetlib_except/exception.h"
#include "larcoreobj/LoggingUtil/Logging.h"

using namespace geo::details;

namespace geo {

  //......................................................................
  unsigned int GeometryIteratorLoopTestAlg::Run()
  {
    /*
     * This monstrous test looks through the elements of the geometry.  It is structured
     * as follows:
     *
     * - loop on cryostats by index
     *   * check global cryostat iterators (ID and element)
     *   - loop on TPCs by index
     *     * check global TPC iterators (ID and element)
     *     * check local TPC iterators (cryostat)
     *     - loop on planes by index
     *       * check global plane iterators (ID and element)
     *       * check local plane iterators (cryostat and TPC)
     *       - loop on wires by index
     *         * check global wire iterators (ID and element)
     *         * check local wire iterators (cryostat, TPC and plane)
     *         * increase wire iterators (also cryostat, TPC and plane locals)
     *       * increase plane iterators (including cryostat and TPC locals)
     *       * loops by range-for with local wire iterators
     *     * increase TPC iterators (including cryostat locals)
     *     * loops by range-for with local plane and wire iterators
     *   * check cryostat-local iterators (TPC, plane, wire IDs and elements)
     *   * loops by range-for with local TPC, plane and wire iterators
     *   - loop on TPC sets by index
     *     * check global TPC set iterators (ID)
     *     - loop on readout planes by index
     *       * check global readout plane iterators (ID)
     *       - loop on channels by channel ID (currently disabled)
     *       * increase readout plane iterators
     *     * check local readout plane iterators (TPC set)
     *     * loops by range-for with local iterators (TPC set)
     *     * increase TPC set iterators
     *   * check local TPC set and readout plane iterators (cryostat)
     *   * loops by range-for with local iterators (TPC set and readout plane)
     * - loops by range-for
     *   * by cryostat ID
     *   * by cryostat
     *   * by TPC ID
     *   * by TPC
     *   * by plane ID
     *   * by plane
     *   * by wire ID
     *   * by wire
     *   * by TPC set ID
     *   * by TPC set
     *   * by readout plane ID
     *   * by readout plane
     *
     * In words: the test is structured in two almost-independent parts.  In the first,
     * nested loops are driven by element indices.  In the second, range-for loops are
     * implemented. The number of loops in here is compared to the number of iterations
     * recorded in the first section (hence the dependence of the second part from the
     * first one).  For the elements with both ID and geometry class (cryostat, TPC, plane
     * and wire) both types of iterators, on ID and on element, are checked, while the
     * ones lacking an element class (TPC set, readout plane) only the ID iterators are
     * tested. The same holds for range-for loops too.
     *
     * In each index loop, a loop of the contained element is nested. Also, the iterators
     * concerning the indexed element are checked. Finally, range-for loops are rolled for
     * iterators local to the element.  For example, each iteration of the TPC loop
     * includes a wire plane loop, a check on TPC iterators (both global and local to the
     * cryostat), and a range-for loop on planes and wires on the TPC.
     *
     * Cryostat loop contains tests for both TPCs and TPC sets.
     */

    unsigned const int nCryo = geom->Ncryostats();
    LAR_LOG_INFO << "We have " << nCryo << " cryostats";

    GeometryIterationPolicy const geoPolicy{geom};
    ReadoutIterationPolicy const readoutPolicy{geom, wireReadoutGeom};

    unsigned int nErrors = 0;
    unsigned int nCryostats = 0, nTPCs = 0, nPlanes = 0, nWires = 0, cumTPCsets = 0, cumROPs = 0;
    cryostat_id_iterator iCryostatID = geom->begin<CryostatID>();
    TPC_id_iterator iTPCID = geom->begin<TPCID>();
    plane_id_iterator iPlaneID = wireReadoutGeom->begin<PlaneID>();
    wire_id_iterator iWireID = wireReadoutGeom->begin<WireID>();

    auto iTPCsetID = wireReadoutGeom->begin<readout::TPCsetID>();
    auto iROPID = wireReadoutGeom->begin<readout::ROPID>();
    // no channel iterator implemented

    cryostat_iterator iCryostat = geom->begin<CryostatGeo>();
    TPC_iterator iTPC = geom->begin<TPCGeo>();
    plane_iterator iPlane = wireReadoutGeom->begin<PlaneGeo>();
    wire_iterator iWire = wireReadoutGeom->begin<WireGeo>();

    CryostatID runningCID{0};
    TPCID runningTID{runningCID, 0};
    PlaneID runningPID{runningTID, 0};
    WireID runningWID{runningPID, 0};
    readout::TPCsetID runningSID{runningCID, 0};
    readout::ROPID runningRID{runningSID, 0};

    for (unsigned int c = 0; c < nCryo; ++c) {
      CryostatID const expCID(c);
      CryostatGeo const& cryo(geom->Cryostat(expCID));
      unsigned const int nTPC = cryo.NTPC();
      unsigned const int nTPCsets = wireReadoutGeom->NTPCsets(expCID);

      LAR_LOG_TRACE
        << "  C=" << c << " (" << nTPC << " TPCs, " << nTPCsets << " TPC sets)";

      if (runningCID != expCID) {
        LAR_LOG_ERROR
          << "Cryostat ID incremented to " << runningCID << ", expected: " << expCID;
        ++nErrors;
      }

      if (iCryostatID->Cryostat != c) {
        LAR_LOG_ERROR
          << "Cryostat ID iterator thinks it's at C=" << (*iCryostatID) << " instead of " << c;
        ++nErrors;
      }

      if (&*iCryostat != &cryo) {
        LAR_LOG_ERROR
          << "Cryostat iterator retrieves CryostatGeo[" << ((void*)iCryostat.get()) << "] ("
          << iCryostat.ID() << ") instead of [" << ((void*)&cryo) << "] (C=" << c << ")";
        ++nErrors;
      }

      TPC_id_iterator iTPCIDinCryo = geom->begin<TPCID>(expCID);
      TPC_iterator iTPCinCryo = geom->begin<TPCGeo>(expCID);
      plane_id_iterator iPlaneIDinCryo = wireReadoutGeom->begin<PlaneID>(expCID);
      plane_iterator iPlaneInCryo = wireReadoutGeom->begin<PlaneGeo>(expCID);
      wire_id_iterator iWireIDinCryo = wireReadoutGeom->begin<WireID>(expCID);
      wire_iterator iWireInCryo = wireReadoutGeom->begin<WireGeo>(expCID);

      unsigned int nPlanesInCryo = 0;
      unsigned int nWiresInCryo = 0;

      for (unsigned int t = 0; t < nTPC; ++t) {
        TPCGeo const& TPC(cryo.TPC(t));
        TPCID const expTID(expCID, t);
        unsigned const int NPlanes = wireReadoutGeom->Nplanes(expTID);

        LAR_LOG_TRACE
          << "    " << expTID << " (" << NPlanes << " planes)";

        if (runningTID != expTID) {
          LAR_LOG_ERROR
            << "TPC ID incremented to " << runningTID << ", expected: " << expTID;
          ++nErrors;
        }

        if (iTPCID->Cryostat != c) {
          LAR_LOG_ERROR
            << "TPC ID iterator thinks it's at C=" << iTPCID->Cryostat << " instead of " << c;
          ++nErrors;
        }
        else if (iTPCID->TPC != t) {
          LAR_LOG_ERROR
            << "TPC ID iterator thinks it's at T=" << iTPCID->TPC << " instead of " << t;
          ++nErrors;
        }

        if (&*iTPC != &TPC) {
          LAR_LOG_ERROR
            << "TPC iterator retrieves TPCGeo[" << ((void*)iTPC.get()) << "] (" << iTPC.ID()
            << ") instead of [" << ((void*)&TPC) << "] (" << expTID << ")";
          ++nErrors;
        }

        if (*iTPCIDinCryo != expTID) {
          LAR_LOG_ERROR
            << "TPC ID local iterator in " << expCID << " points to " << *iTPCIDinCryo
            << " instead of " << expTID;
          ++nErrors;
        }
        if (iTPCinCryo->ID() != expTID) {
          LAR_LOG_ERROR
            << "TPC local iterator in " << expCID << " points to " << iTPCinCryo->ID()
            << " instead of " << expTID;
          ++nErrors;
        }

        plane_id_iterator iPlaneIDinTPC = wireReadoutGeom->begin<PlaneID>(expTID);
        plane_iterator iPlaneInTPC = wireReadoutGeom->begin<PlaneGeo>(expTID);
        wire_id_iterator iWireIDinTPC = wireReadoutGeom->begin<WireID>(expTID);
        wire_iterator iWireInTPC = wireReadoutGeom->begin<WireGeo>(expTID);

        unsigned int nPlanesInTPC = 0;
        unsigned int nWiresInTPC = 0;

        for (unsigned int p = 0; p < NPlanes; ++p) {
          PlaneID const expPID(expTID, p);
          PlaneGeo const& Plane(wireReadoutGeom->Plane(expPID));
          unsigned const int NWires = Plane.Nwires();

          LAR_LOG_TRACE
            << "    " << expTID << " (" << NWires << " wires)";

          if (runningPID != expPID) {
            LAR_LOG_ERROR
              << "Plane ID incremented to " << runningPID << ", expected: " << expPID;
            ++nErrors;
          }
          if (iPlaneID->Cryostat != c) {
            LAR_LOG_ERROR
              << "plane ID iterator thinks it's at C=" << iPlaneID->Cryostat << " instead of " << c;
            ++nErrors;
          }
          else if (iPlaneID->TPC != t) {
            LAR_LOG_ERROR
              << "plane ID iterator thinks it's at T=" << iPlaneID->TPC << " instead of " << t;
            ++nErrors;
          }
          else if (iPlaneID->Plane != p) {
            LAR_LOG_ERROR
              << "plane ID iterator thinks it's at P=" << iPlaneID->Plane << " instead of " << p;
            ++nErrors;
          }

          if (&*iPlane != &Plane) {
            LAR_LOG_ERROR
              << "plane iterator retrieves PlaneGeo[" << ((void*)iPlane.get()) << "] instead of ["
              << ((void*)&Plane) << "] (" << expPID << ")";
            ++nErrors;
          }

          if (*iPlaneIDinCryo != expPID) {
            LAR_LOG_ERROR
              << "Plane ID local iterator in " << expCID << " points to " << *iPlaneIDinCryo
              << " instead of " << expPID;
            ++nErrors;
          }
          if (iPlaneInCryo->ID() != expPID) {
            LAR_LOG_ERROR
              << "Plane local iterator in " << expCID << " points to " << iPlaneInCryo.ID()
              << " instead of " << expPID;
            ++nErrors;
          }

          if (*iPlaneIDinTPC != expPID) {
            LAR_LOG_ERROR
              << "Plane ID local iterator in " << expTID << " points to " << *iPlaneIDinTPC
              << " instead of " << expPID;
            ++nErrors;
          }
          if (iPlaneInTPC->ID() != expPID) {
            LAR_LOG_ERROR
              << "Plane local iterator in " << expTID << " points to " << iPlaneInTPC.ID()
              << " instead of " << expPID;
            ++nErrors;
          }

          wire_id_iterator iWireIDinPlane = wireReadoutGeom->begin<WireID>(expPID);
          wire_iterator iWireInPlane = wireReadoutGeom->begin<WireGeo>(expPID);

          unsigned int nWiresInPlane = 0; // will become same as NWires

          for (unsigned int w = 0; w < NWires; ++w) {
            WireGeo const& Wire(Plane.Wire(w));
            WireID const expWID(expPID, w);

            if (runningWID != expWID) {
              LAR_LOG_ERROR
                << "Wire ID incremented to " << runningWID << ", expected: " << expWID;
              ++nErrors;
            }

            LAR_LOG_TRACE << "    " << expWID;
            if (iWireID->Cryostat != c) {
              LAR_LOG_ERROR
                << "wire ID iterator thinks it's at C=" << iWireID->Cryostat << " instead of " << c;
              ++nErrors;
            }
            else if (iWireID->TPC != t) {
              LAR_LOG_ERROR
                << "wire ID iterator thinks it's at T=" << iWireID->TPC << " instead of " << t;
              ++nErrors;
            }
            else if (iWireID->Plane != p) {
              LAR_LOG_ERROR
                << "wire ID iterator thinks it's at P=" << iWireID->Plane << " instead of " << p;
              ++nErrors;
            }
            else if (iWireID->Wire != w) {
              LAR_LOG_ERROR
                << "wire ID iterator thinks it's at W=" << iWireID->Wire << " instead of " << w;
              ++nErrors;
            }

            if (&*iWire != &Wire) {
              LAR_LOG_ERROR
                << "wire iterator retrieves WireGeo[" << ((void*)iWire.get()) << "] instead of ["
                << ((void*)&Wire) << "] (" << expWID << ")";
              ++nErrors;
            }

            if (*iWireIDinCryo != expWID) {
              LAR_LOG_ERROR
                << "Wire ID local iterator in " << expCID << " points to " << *iWireIDinCryo
                << " instead of " << expWID;
              ++nErrors;
            }
            if (iWireInCryo.ID() != expWID) {
              LAR_LOG_ERROR
                << "Wire local iterator in " << expCID << " points to " << iWireInCryo.ID()
                << " instead of " << expWID;
              ++nErrors;
            }
            if (*iWireIDinTPC != expWID) {
              LAR_LOG_ERROR
                << "Wire ID local iterator in " << expTID << " points to " << *iWireIDinTPC
                << " instead of " << expWID;
              ++nErrors;
            }
            if (iWireInTPC.ID() != expWID) {
              LAR_LOG_ERROR
                << "Wire local iterator in " << expTID << " points to " << iWireInTPC.ID()
                << " instead of " << expWID;
              ++nErrors;
            }
            if (*iWireIDinPlane != expWID) {
              LAR_LOG_ERROR
                << "Wire ID local iterator in " << expPID << " points to " << *iWireIDinPlane
                << " instead of " << expWID;
              ++nErrors;
            }
            if (iWireInPlane.ID() != expWID) {
              LAR_LOG_ERROR
                << "Wire local iterator in " << expPID << " points to " << iWireInPlane.ID()
                << " instead of " << expWID;
              ++nErrors;
            }

            ++iWireID;
            ++iWireIDinCryo;
            ++iWireIDinTPC;
            ++iWireIDinPlane;
            ++iWire;
            ++iWireInCryo;
            ++iWireInTPC;
            ++iWireInPlane;
            ++nWires;
            ++nWiresInCryo;
            ++nWiresInTPC;
            ++nWiresInPlane;
            IncrementID(runningWID, readoutPolicy);
          } // end loop over wires

          if (iWireIDinPlane != wireReadoutGeom->end<WireID>(expPID)) {
            LAR_LOG_ERROR
              << "Wire ID local iterator in " << expPID
              << " should be at end and instead points to " << *iWireIDinPlane;
            ++nErrors;
          }
          if (iWireInPlane != wireReadoutGeom->end<WireGeo>(expPID)) {
            LAR_LOG_ERROR
              << "Wire local iterator in " << expPID << " should be at end, and instead points to "
              << iWireInPlane.ID();
            ++nErrors;
          }

          // test if we can loop all wires in this plane via iterator box
          LAR_LOG_DEBUG
            << "Looping though " << nWiresInPlane << " wires in " << expPID;
          unsigned int nLoopedWireIDs = 0;
          for (auto const& wID : wireReadoutGeom->Iterate<WireID>(expPID)) {
            LAR_LOG_TRACE << wID;
            if (nLoopedWireIDs >= nWiresInPlane) {
              LAR_LOG_ERROR
                << "After all " << nLoopedWireIDs << " wire IDs in " << expPID
                << ", iterator has not reached the end but it's still at " << wID;
              ++nErrors;
              break;
            }
            ++nLoopedWireIDs;
          }
          if (nLoopedWireIDs < nWiresInPlane) {
            LAR_LOG_ERROR
              << "Looped only " << nLoopedWireIDs << " wire IDs in " << expPID
              << ", while we expected " << nWiresInPlane << " iterations!";
            ++nErrors;
          } // if
          unsigned int nLoopedWires = 0;
          for (auto const& wire [[maybe_unused]] : wireReadoutGeom->Iterate<WireGeo>(expPID)) {
            if (nLoopedWires >= nWiresInPlane) {
              LAR_LOG_ERROR
                << "After all " << nLoopedWires << " wires in " << expPID
                << ", iterator has not reached the end";
              ++nErrors;
              break;
            }
            ++nLoopedWires;
          }
          if (nLoopedWires < nWiresInPlane) {
            LAR_LOG_ERROR
              << "Looped only " << nLoopedWires << " wires in " << expPID << ", while we expected "
              << nWiresInPlane << " iterations!";
            ++nErrors;
          } // if

          ++iPlaneID;
          ++iPlaneIDinCryo;
          ++iPlaneIDinTPC;
          ++iPlane;
          ++iPlaneInCryo;
          ++iPlaneInTPC;
          ++nPlanes;
          ++nPlanesInCryo;
          ++nPlanesInTPC;
          IncrementID(runningPID, readoutPolicy);
        } // end loop over planes

        if (iPlaneIDinTPC != wireReadoutGeom->end<PlaneID>(expTID)) {
          LAR_LOG_ERROR
            << "Plane ID local iterator in " << expTID << " should be at end and instead points to "
            << *iPlaneIDinTPC;
          ++nErrors;
        }
        if (iPlaneInTPC != wireReadoutGeom->end<PlaneGeo>(expTID)) {
          LAR_LOG_ERROR
            << "Plane local iterator in " << expTID << " should be at end, and instead points to "
            << iPlaneInTPC.ID();
          ++nErrors;
        }

        if (iWireIDinTPC != wireReadoutGeom->end<WireID>(expTID)) {
          LAR_LOG_ERROR
            << "Wire ID local iterator in " << expTID << " should be at end and instead points to "
            << *iWireIDinTPC;
          ++nErrors;
        }
        if (iWireInTPC != wireReadoutGeom->end<WireGeo>(expTID)) {
          LAR_LOG_ERROR
            << "Wire local iterator in " << expTID << " should be at end, and instead points to "
            << iWireInTPC.ID();
          ++nErrors;
        }

        // test if we can loop all planes in this TPC via iterator box
        LAR_LOG_DEBUG
          << "Looping though " << nPlanesInTPC << " planes in " << expTID;
        unsigned int nLoopedPlaneIDs = 0;
        for (auto const& pID : wireReadoutGeom->Iterate<PlaneID>(expTID)) {
          LAR_LOG_TRACE << pID;
          if (nLoopedPlaneIDs >= nPlanesInTPC) {
            LAR_LOG_ERROR
              << "After all " << nLoopedPlaneIDs << " plane IDs in " << expTID
              << ", iterator has not reached the end but it's still at " << pID;
            ++nErrors;
            break;
          }
          ++nLoopedPlaneIDs;
        }
        if (nLoopedPlaneIDs < nPlanesInTPC) {
          LAR_LOG_ERROR
            << "Looped only " << nLoopedPlaneIDs << " plane IDs in " << expTID
            << ", while we expected " << nPlanesInTPC << " iterations!";
          ++nErrors;
        } // if
        unsigned int nLoopedPlanes = 0;
        for (auto const& plane [[maybe_unused]] : wireReadoutGeom->Iterate<PlaneGeo>(expTID)) {
          if (nLoopedPlanes >= nPlanesInTPC) {
            LAR_LOG_ERROR
              << "After all " << nLoopedPlanes << " planes in " << expTID
              << ", iterator has not reached the end";
            ++nErrors;
            break;
          }
          ++nLoopedPlanes;
        }
        if (nLoopedPlanes < nPlanesInTPC) {
          LAR_LOG_ERROR
            << "Looped only " << nLoopedPlanes << " planes in " << expTID << ", while we expected "
            << nPlanesInTPC << " iterations!";
          ++nErrors;
        } // if

        // test if we can loop all wires in this TPC via iterator box
        LAR_LOG_DEBUG
          << "Looping though " << nWiresInTPC << " wires in " << expTID;
        unsigned int nLoopedWireIDs = 0;
        for (auto const& wID : wireReadoutGeom->Iterate<WireID>(expTID)) {
          LAR_LOG_TRACE << wID;
          if (nLoopedWireIDs >= nWiresInTPC) {
            LAR_LOG_ERROR
              << "After all " << nLoopedWireIDs << " wire IDs in " << expTID
              << ", iterator has not reached the end but it's still at " << wID;
            ++nErrors;
            break;
          }
          ++nLoopedWireIDs;
        }
        if (nLoopedWireIDs < nWiresInTPC) {
          LAR_LOG_ERROR
            << "Looped only " << nLoopedWireIDs << " wire IDs in " << expTID
            << ", while we expected " << nWiresInTPC << " iterations!";
          ++nErrors;
        } // if
        unsigned int nLoopedWires = 0;
        for (WireGeo const& wire [[maybe_unused]] : wireReadoutGeom->Iterate<WireGeo>(expTID)) {
          if (nLoopedWires >= nWiresInTPC) {
            LAR_LOG_ERROR
              << "After all " << nLoopedWires << " wires in " << expTID
              << ", iterator has not reached the end";
            ++nErrors;
            break;
          }
          ++nLoopedWires;
        }
        if (nLoopedWires < nWiresInTPC) {
          LAR_LOG_ERROR
            << "Looped only " << nLoopedWires << " wires in " << expTID << ", while we expected "
            << nWiresInTPC << " iterations!";
          ++nErrors;
        } // if

        ++iTPCID;
        ++iTPC;
        ++iTPCIDinCryo;
        ++iTPCinCryo;
        ++nTPCs;
        IncrementID(runningTID, geoPolicy);
      } // end loop over tpcs

      if (iTPCIDinCryo != geom->end<TPCID>(expCID)) {
        LAR_LOG_ERROR
          << "TPC ID local iterator in " << expCID << " should be at end, and instead points to "
          << *iTPCIDinCryo;
        ++nErrors;
      }
      if (iTPCinCryo != geom->end<TPCGeo>(expCID)) {
        LAR_LOG_ERROR
          << "TPC local iterator in " << expCID << " should be at end, and instead points to "
          << iTPCinCryo->ID();
        ++nErrors;
      }

      if (iPlaneIDinCryo != wireReadoutGeom->end<PlaneID>(expCID)) {
        LAR_LOG_ERROR
          << "Plane ID local iterator in " << expCID << " should be at end, and instead points to "
          << *iPlaneIDinCryo;
        ++nErrors;
      }
      if (iPlaneInCryo != wireReadoutGeom->end<PlaneGeo>(expCID)) {
        LAR_LOG_ERROR
          << "Plane local iterator in " << expCID << " should be at end, and instead points to "
          << iPlaneInCryo->ID();
        ++nErrors;
      }

      if (iWireIDinCryo != wireReadoutGeom->end<WireID>(expCID)) {
        LAR_LOG_ERROR
          << "Wire ID local iterator in " << expCID << " should be at end, and instead points to "
          << *iWireIDinCryo;
        ++nErrors;
      }
      if (iWireInCryo != wireReadoutGeom->end<WireGeo>(expCID)) {
        LAR_LOG_ERROR
          << "Wire local iterator in " << expCID << " should be at end, and instead points to "
          << iWireInCryo.ID();
        ++nErrors;
      }

      // test if we can loop all TPCs in this cryostat via iterator box
      unsigned int nTPCsInCryo = cryo.NTPC();
      LAR_LOG_DEBUG
        << "Looping though " << nTPCsInCryo << " TPCs in " << expCID;
      unsigned int nLoopedTPCIDs = 0;
      for (TPCID const& tID : geom->Iterate<TPCID>(expCID)) {
        LAR_LOG_TRACE << tID;
        if (nLoopedTPCIDs >= nTPCsInCryo) {
          LAR_LOG_ERROR
            << "After all " << nLoopedTPCIDs << " TPC IDs in " << expCID
            << ", iterator has not reached the end but it's still at " << tID;
          ++nErrors;
          break;
        }
        ++nLoopedTPCIDs;
      }
      if (nLoopedTPCIDs < nTPCsInCryo) {
        LAR_LOG_ERROR
          << "Looped only " << nLoopedTPCIDs << " TPC IDs in " << expCID << ", while we expected "
          << nTPCsInCryo << " iterations!";
        ++nErrors;
      } // if
      unsigned int nLoopedTPCs = 0;
      for (TPCGeo const& TPC [[maybe_unused]] : geom->Iterate<TPCGeo>(expCID)) {
        if (nLoopedTPCs >= nTPCsInCryo) {
          LAR_LOG_ERROR
            << "After all " << nLoopedTPCs << " TPCs in " << expCID
            << ", iterator has not reached the end";
          ++nErrors;
          break;
        }
        ++nLoopedTPCs;
      }
      if (nLoopedTPCs < nTPCsInCryo) {
        LAR_LOG_ERROR
          << "Looped only " << nLoopedTPCs << " TPCs in " << expCID << ", while we expected "
          << nTPCsInCryo << " iterations!";
        ++nErrors;
      } // if

      // test if we can loop all planes in this cryostat via iterator box
      LAR_LOG_DEBUG
        << "Looping though " << nPlanesInCryo << " planes in " << expCID;
      unsigned int nLoopedPlaneIDs = 0;
      for (PlaneID const& pID : wireReadoutGeom->Iterate<PlaneID>(expCID)) {
        LAR_LOG_TRACE << pID;
        if (nLoopedPlaneIDs >= nPlanesInCryo) {
          LAR_LOG_ERROR
            << "After all " << nLoopedPlaneIDs << " plane IDs in " << expCID
            << ", iterator has not reached the end but it's still at " << pID;
          ++nErrors;
          break;
        }
        ++nLoopedPlaneIDs;
      }
      if (nLoopedPlaneIDs < nPlanesInCryo) {
        LAR_LOG_ERROR
          << "Looped only " << nLoopedPlaneIDs << " plane IDs in " << expCID
          << ", while we expected " << nPlanesInCryo << " iterations!";
        ++nErrors;
      } // if
      unsigned int nLoopedPlanes = 0;
      for (PlaneGeo const& plane [[maybe_unused]] : wireReadoutGeom->Iterate<PlaneGeo>(expCID)) {
        if (nLoopedPlanes >= nPlanesInCryo) {
          LAR_LOG_ERROR
            << "After all " << nLoopedPlanes << " planes in " << expCID
            << ", iterator has not reached the end";
          ++nErrors;
          break;
        }
        ++nLoopedPlanes;
      }
      if (nLoopedPlanes < nPlanesInCryo) {
        LAR_LOG_ERROR
          << "Looped only " << nLoopedPlanes << " planes in " << expCID << ", while we expected "
          << nPlanesInCryo << " iterations!";
        ++nErrors;
      } // if

      // test if we can loop all wires in this cryostat via iterator box
      LAR_LOG_DEBUG
        << "Looping though " << nWiresInCryo << " wires in " << expCID;
      unsigned int nLoopedWireIDs = 0;
      for (WireID const& wID : wireReadoutGeom->Iterate<WireID>(expCID)) {
        LAR_LOG_TRACE << wID;
        if (nLoopedWireIDs >= nWiresInCryo) {
          LAR_LOG_ERROR
            << "After all " << nLoopedWireIDs << " wire IDs in " << expCID
            << ", iterator has not reached the end but it's still at " << wID;
          ++nErrors;
          break;
        }
        ++nLoopedWireIDs;
      }
      if (nLoopedWireIDs < nWiresInCryo) {
        LAR_LOG_ERROR
          << "Looped only " << nLoopedWireIDs << " wire IDs in " << expCID << ", while we expected "
          << nWiresInCryo << " iterations!";
        ++nErrors;
      } // if
      unsigned int nLoopedWires = 0;
      for (WireGeo const& wire [[maybe_unused]] : wireReadoutGeom->Iterate<WireGeo>(expCID)) {
        if (nLoopedWires >= nWiresInCryo) {
          LAR_LOG_ERROR
            << "After all " << nLoopedWires << " wires in " << expCID
            << ", iterator has not reached the end";
          ++nErrors;
          break;
        }
        ++nLoopedWires;
      }
      if (nLoopedWires < nWiresInCryo) {
        LAR_LOG_ERROR
          << "Looped only " << nLoopedWires << " wires in " << expCID << ", while we expected "
          << nWiresInCryo << " iterations!";
        ++nErrors;
      } // if

      // readout iterators
      TPCset_id_iterator iTPCsetIDinCryo = wireReadoutGeom->begin<readout::TPCsetID>(expCID);
      ROP_id_iterator iROPIDinCryo = wireReadoutGeom->begin<readout::ROPID>(expCID);

      unsigned int nTPCsetsInCryo = 0;
      unsigned int nROPInCryo = 0;

      for (unsigned int s = 0; s < nTPCsets; ++s) {
        readout::TPCsetID const expSID(expCID, s);
        unsigned const int NROPs = wireReadoutGeom->NROPs(expSID);

        LAR_LOG_TRACE << "    " << expSID << " (" << NROPs << " planes)";

        if (runningSID != expSID) {
          LAR_LOG_ERROR
            << "TPC set ID incremented to " << runningSID << ", expected: " << expSID;
          ++nErrors;
        }

        if (iTPCsetID->Cryostat != c) {
          LAR_LOG_ERROR
            << "TPC set ID iterator thinks it's at C=" << iTPCsetID->Cryostat << " instead of "
            << c;
          ++nErrors;
        }
        else if (iTPCsetID->TPCset != s) {
          LAR_LOG_ERROR
            << "TPC set ID iterator thinks it's at S=" << iTPCsetID->TPCset << " instead of " << s;
          ++nErrors;
        }

        if (*iTPCsetIDinCryo != expSID) {
          LAR_LOG_ERROR
            << "TPC set ID local iterator in " << expCID << " points to " << *iTPCsetIDinCryo
            << " instead of " << expSID;
          ++nErrors;
        }

        ROP_id_iterator iROPIDinTPCset = wireReadoutGeom->begin<readout::ROPID>(expSID);

        unsigned int nROPInTPCset = 0; // this will become NROPs

        for (unsigned int r = 0; r < NROPs; ++r) {
          readout::ROPID const expRID(expSID, r);
          unsigned const int NChannels = wireReadoutGeom->Nchannels(expRID);

          LAR_LOG_TRACE
            << "    " << expRID << " (" << NChannels << " channels)";

          if (runningRID != expRID) {
            LAR_LOG_ERROR
              << "Readout plane ID incremented to " << runningRID << ", expected: " << expRID;
            ++nErrors;
          }

          if (iROPID->Cryostat != c) {
            LAR_LOG_ERROR
              << "readout plane ID iterator thinks it's at C=" << iROPID->Cryostat << " instead of "
              << c;
            ++nErrors;
          }
          else if (iROPID->TPCset != s) {
            LAR_LOG_ERROR
              << "readout plane ID iterator thinks it's at S=" << iROPID->TPCset << " instead of "
              << s;
            ++nErrors;
          }
          else if (iROPID->ROP != r) {
            LAR_LOG_ERROR
              << "readout plane ID iterator thinks it's at R=" << iROPID->ROP << " instead of "
              << r;
            ++nErrors;
          }

          if (*iROPIDinCryo != expRID) {
            LAR_LOG_ERROR
              << "Readout plane ID local iterator in " << expCID << " points to " << *iROPIDinCryo
              << " instead of " << expRID;
            ++nErrors;
          }

          if (*iROPIDinTPCset != expRID) {
            LAR_LOG_ERROR
              << "Readout plane ID local iterator in " << expSID << " points to " << *iROPIDinTPCset
              << " instead of " << expRID;
            ++nErrors;
          }

          ++iROPID;
          ++iROPIDinCryo;
          ++iROPIDinTPCset;
          ++cumROPs;
          ++nROPInCryo;
          ++nROPInTPCset;
          IncrementID(runningRID, readoutPolicy);
        } // end loop over readout planes

        if (iROPIDinTPCset != wireReadoutGeom->end<readout::ROPID>(expSID)) {
          LAR_LOG_ERROR
            << "Readout plane ID local iterator in " << expSID
            << " should be at end and instead points to " << *iROPIDinTPCset;
          ++nErrors;
        }

        // test if we can loop all ROPs in this TPC set via iterator box
        LAR_LOG_DEBUG
          << "Looping though " << nROPInTPCset << " readout planes in " << expSID;
        unsigned int nLoopedROPIDs = 0;
        for (readout::ROPID const& rID : wireReadoutGeom->Iterate<readout::ROPID>(expSID)) {
          LAR_LOG_TRACE << rID;
          if (nLoopedROPIDs >= nROPInTPCset) {
            LAR_LOG_ERROR
              << "After all " << nLoopedROPIDs << " readout plane IDs in " << expSID
              << ", iterator has not reached the end but it's still at " << rID;
            ++nErrors;
            break;
          }
          ++nLoopedROPIDs;
        }
        if (nLoopedROPIDs < nROPInTPCset) {
          LAR_LOG_ERROR
            << "Looped only " << nLoopedROPIDs << " readout plane IDs in " << expSID
            << ", while we expected " << nROPInTPCset << " iterations!";
          ++nErrors;
        } // if

        ++iTPCsetID;
        ++iTPCsetIDinCryo;
        ++cumTPCsets;
        ++nTPCsetsInCryo;
        IncrementID(runningSID, readoutPolicy);
      } // end loop over TPC sets

      if (iTPCsetIDinCryo != wireReadoutGeom->end<readout::TPCsetID>(expCID)) {
        LAR_LOG_ERROR
          << "TPC set ID local iterator in " << expCID
          << " should be at end, and instead points to " << *iTPCsetIDinCryo;
        ++nErrors;
      }

      if (iROPIDinCryo != wireReadoutGeom->end<readout::ROPID>(expCID)) {
        LAR_LOG_ERROR
          << "Readout plane ID local iterator in " << expCID
          << " should be at end, and instead points to " << *iROPIDinCryo;
        ++nErrors;
      }

      // test if we can loop all TPC sets in this cryostat via iterator box
      LAR_LOG_DEBUG
        << "Looping though " << nTPCsetsInCryo << " TPC sets in " << expCID;
      unsigned int nLoopedTPCsetIDs = 0;
      for (readout::TPCsetID const& sID : wireReadoutGeom->Iterate<readout::TPCsetID>(expCID)) {
        LAR_LOG_TRACE << sID;
        if (nLoopedTPCsetIDs >= nTPCsetsInCryo) {
          LAR_LOG_ERROR
            << "After all " << nLoopedTPCsetIDs << " TPC set IDs in " << expCID
            << ", iterator has not reached the end but it's still at " << sID;
          ++nErrors;
          break;
        }
        ++nLoopedTPCsetIDs;
      }
      if (nLoopedTPCsetIDs < nTPCsetsInCryo) {
        LAR_LOG_ERROR
          << "Looped only " << nLoopedTPCsetIDs << " TPC set IDs in " << expCID
          << ", while we expected " << nTPCsetsInCryo << " iterations!";
        ++nErrors;
      } // if

      // test if we can loop all readout planes in this cryostat via iterator box
      LAR_LOG_DEBUG
        << "Looping though " << nROPInCryo << " readout planes in " << expCID;
      unsigned int nLoopedROPIDs = 0;
      for (readout::ROPID const& rID : wireReadoutGeom->Iterate<readout::ROPID>(expCID)) {
        LAR_LOG_TRACE << rID;
        if (nLoopedROPIDs >= nROPInCryo) {
          LAR_LOG_ERROR
            << "After all " << nLoopedROPIDs << " readout plane IDs in " << expCID
            << ", iterator has not reached the end but it's still at " << rID;
          ++nErrors;
          break;
        }
        ++nLoopedROPIDs;
      }
      if (nLoopedROPIDs < nROPInCryo) {
        LAR_LOG_ERROR
          << "Looped only " << nLoopedROPIDs << " readout plane IDs in " << expCID
          << ", while we expected " << nROPInCryo << " iterations!";
        ++nErrors;
      } // if

      ++iCryostatID;
      ++iCryostat;
      ++nCryostats;
      IncrementID(runningCID, geoPolicy);
    } // end loop over cryostats

    try {
      CryostatGeo const& Cryo [[maybe_unused]] = *iCryostat;
      LAR_LOG_ERROR << "Cryostat iterator thinks it's still at "
                                               << iCryostat.ID() << ", but we are already finished";
      ++nErrors;
    }
    catch (cet::exception const&) {
      LAR_LOG_DEBUG
        << "exception caught"
           " while dereferencing an iterator to a past-the-end cryostat.\n";
    }

    // test if we can loop all cryostats with the iterators (via iterator box)
    LAR_LOG_DEBUG << "Looping though " << nCryostats << " cryostats";
    unsigned int nLoopedCryostatIDs = 0;
    for (CryostatID const& cID : geom->Iterate<CryostatID>()) {
      LAR_LOG_TRACE << cID;
      if (nLoopedCryostatIDs >= nCryostats) {
        LAR_LOG_ERROR
          << "After all " << nLoopedCryostatIDs
          << " cryostat IDs, iterator has not reached the end but it's still at " << cID;
        ++nErrors;
        break;
      }
      ++nLoopedCryostatIDs;
    }
    if (nLoopedCryostatIDs < nCryostats) {
      LAR_LOG_ERROR
        << "Looped only " << nLoopedCryostatIDs << " cryostat IDs, while we expected " << nCryostats
        << " iterations!";
      ++nErrors;
    } // if
    unsigned int nLoopedCryostats = 0;
    for (CryostatGeo const& Cryo [[maybe_unused]] : geom->Iterate<CryostatGeo>()) {
      if (nLoopedCryostats >= nCryostats) {
        LAR_LOG_ERROR
          << "After all " << nLoopedCryostats << " cryostats, iterator has not reached the end";
        ++nErrors;
        break;
      }
      ++nLoopedCryostats;
    }
    if (nLoopedCryostats < nCryostats) {
      LAR_LOG_ERROR
        << "Looped only " << nLoopedCryostats << " cryostats, while we expected " << nCryostats
        << " iterations!";
      ++nErrors;
    } // if

    try {
      TPCGeo const& TPC [[maybe_unused]] = *iTPC;
      LAR_LOG_ERROR
        << "TPC iterator thinks it's still at " << iTPC.ID() << ", but we are already finished";
      ++nErrors;
    }
    catch (cet::exception const&) {
      LAR_LOG_DEBUG
        << "exception caught"
           " while dereferencing an iterator to a past-the-end TPC.\n";
    }

    // test if we can loop all TPCs with the iterators (via iterator box)
    LAR_LOG_DEBUG << "Looping though " << nTPCs << " TPCs";
    unsigned int nLoopedTPCIDs = 0;
    for (TPCID const& tID : geom->Iterate<TPCID>()) {
      LAR_LOG_TRACE << tID;
      if (nLoopedTPCIDs >= nTPCs) {
        LAR_LOG_ERROR
          << "After all " << nLoopedTPCIDs
          << " TPC IDs, iterator has not reached the end but it's still at " << tID;
        ++nErrors;
        break;
      }
      ++nLoopedTPCIDs;
    }
    if (nLoopedTPCIDs < nTPCs) {
      LAR_LOG_ERROR
        << "Looped only " << nLoopedTPCIDs << " TPC IDs, while we expected " << nTPCs
        << " iterations!";
      ++nErrors;
    } // if
    unsigned int nLoopedTPCs = 0;
    for (TPCGeo const& TPC [[maybe_unused]] : geom->Iterate<TPCGeo>()) {
      if (nLoopedTPCs >= nTPCs) {
        LAR_LOG_ERROR
          << "After all " << nLoopedTPCs << " TPCs, iterator has not reached the end";
        ++nErrors;
        break;
      }
      ++nLoopedTPCs;
    }
    if (nLoopedTPCs < nTPCs) {
      LAR_LOG_ERROR
        << "Looped only " << nLoopedTPCs << " TPCs, while we expected " << nTPCs << " iterations!";
      ++nErrors;
    } // if

    try {
      PlaneGeo const& Plane [[maybe_unused]] = *iPlane;
      LAR_LOG_ERROR
        << "Plane iterator thinks it's still at " << iPlane.ID() << ", but we are already finished";
      ++nErrors;
    }
    catch (cet::exception const&) {
      LAR_LOG_DEBUG
        << "exception caught"
           " while dereferencing an iterator to a past-the-end plane.\n";
    }

    // test if we can loop all planes with the iterators (via iterator box)
    LAR_LOG_DEBUG << "Looping though " << nPlanes << " planes";
    unsigned int nLoopedPlaneIDs = 0;
    for (PlaneID const& pID : wireReadoutGeom->Iterate<PlaneID>()) {
      LAR_LOG_TRACE << pID;
      if (nLoopedPlaneIDs >= nPlanes) {
        LAR_LOG_ERROR
          << "After all " << nLoopedPlaneIDs
          << " planes, ID iterator has not reached the end but it's still at " << pID;
        ++nErrors;
        break;
      }
      ++nLoopedPlaneIDs;
    }
    if (nLoopedPlaneIDs < nPlanes) {
      LAR_LOG_ERROR
        << "Looped only " << nLoopedPlaneIDs << " plane IDs, while we expected " << nPlanes
        << " iterations!";
      ++nErrors;
    } // if
    unsigned int nLoopedPlanes = 0;
    for (PlaneGeo const& Plane [[maybe_unused]] : wireReadoutGeom->Iterate<PlaneGeo>()) {
      if (nLoopedPlanes >= nPlanes) {
        LAR_LOG_ERROR
          << "After all " << nLoopedPlanes << " planes, iterator has not reached the end";
        ++nErrors;
        break;
      }
      ++nLoopedPlanes;
    }
    if (nLoopedPlanes < nPlanes) {
      LAR_LOG_ERROR
        << "Looped only " << nLoopedPlanes << " planes, while we expected " << nPlanes
        << " iterations!";
      ++nErrors;
    } // if

    try {
      WireGeo const& Wire [[maybe_unused]] = *iWire;
      LAR_LOG_ERROR
        << "Wire iterator thinks it's still at " << iWire.ID() << ", but we are already finished";
      ++nErrors;
    }
    catch (cet::exception const&) {
      LAR_LOG_DEBUG
        << "exception caught"
           " while dereferencing an iterator to a past-the-end wire.\n";
    }

    // test if we can loop all wires with the iterators (via iterator box)
    LAR_LOG_DEBUG << "Looping though " << nWires << " wires";
    unsigned int nLoopedWireIDs = 0;
    for (WireID const& wID : wireReadoutGeom->Iterate<WireID>()) {
      LAR_LOG_TRACE << wID;
      if (nLoopedWireIDs >= nWires) {
        LAR_LOG_ERROR
          << "After all " << nLoopedWireIDs
          << " wire IDs, iterator has not reached the end but it's still at " << wID;
        ++nErrors;
        break;
      }
      ++nLoopedWireIDs;
    }
    if (nLoopedWireIDs < nWires) {
      LAR_LOG_ERROR
        << "Looped only " << nLoopedWireIDs << " wire IDs, while we expected " << nWires
        << " iterations!";
      ++nErrors;
    } // if
    unsigned int nLoopedWires = 0;
    for (WireGeo const& Wire [[maybe_unused]] : wireReadoutGeom->Iterate<WireGeo>()) {
      if (nLoopedWires >= nWires) {
        LAR_LOG_ERROR
          << "After all " << nLoopedWires << " wires, iterator has not reached the end";
        ++nErrors;
        break;
      }
      ++nLoopedWires;
    }
    if (nLoopedWires < nWires) {
      LAR_LOG_ERROR
        << "Looped only " << nLoopedWires << " wires, while we expected " << nWires
        << " iterations!";
      ++nErrors;
    } // if

    // test if we can loop all TPC sets with the iterators (via iterator box)
    LAR_LOG_DEBUG << "Looping though " << cumTPCsets << " TPC sets";
    unsigned int nLoopedTPCsetIDs = 0;
    for (readout::TPCsetID const& sID : wireReadoutGeom->Iterate<readout::TPCsetID>()) {
      LAR_LOG_TRACE << sID;
      if (nLoopedTPCsetIDs >= cumTPCsets) {
        LAR_LOG_ERROR
          << "After all " << nLoopedTPCsetIDs
          << " TPC set IDs, iterator has not reached the end but it's still at" << sID;
        ++nErrors;
        break;
      }
      ++nLoopedTPCsetIDs;
    }
    if (nLoopedTPCsetIDs < cumTPCsets) {
      LAR_LOG_ERROR
        << "Looped only " << nLoopedTPCsetIDs << " TPC set IDs, while we expected " << cumTPCsets
        << " iterations!";
      ++nErrors;
    } // if

    // test if we can loop all planes with the iterators (via iterator box)
    LAR_LOG_DEBUG << "Looping though " << cumROPs << " readout planes";
    unsigned int nLoopedROPIDs = 0;
    for (readout::ROPID const& rID : wireReadoutGeom->Iterate<readout::ROPID>()) {
      LAR_LOG_TRACE << rID;
      if (nLoopedROPIDs >= cumROPs) {
        LAR_LOG_ERROR
          << "After all " << nLoopedROPIDs
          << " readout planes, ID iterator has not reached the end but it's still at" << rID;
        ++nErrors;
        break;
      }
      ++nLoopedROPIDs;
    }
    if (nLoopedROPIDs < cumROPs) {
      LAR_LOG_ERROR
        << "Looped only " << nLoopedROPIDs << " readout plane IDs, while we expected " << cumROPs
        << " iterations!";
      ++nErrors;
    } // if

    return nErrors;
  } // GeometryIteratorLoopTestAlg::Run()

  //----------------------------------------------------------------------------

} // namespace geo
