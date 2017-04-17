/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2017,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "base.hpp"

namespace ndn {
namespace tools {
namespace autoconfig {

Base::Base(Face& face, KeyChain& keyChain, const NextStageCallback& nextStageOnFailure)
  : m_face(face)
  , m_keyChain(keyChain)
  , m_controller(face, keyChain)
  , m_nextStageOnFailure(nextStageOnFailure)
{
}

void
Base::connectToHub(const std::string& uri)
{
  FaceUri faceUri(uri);
  std::cerr << "About to connect to: " << uri << std::endl;

  faceUri.canonize(bind(&Base::onCanonizeSuccess, this, _1),
                   bind(&Base::onCanonizeFailure, this, _1),
                   m_face.getIoService(), time::seconds(4));

}


void
Base::onCanonizeSuccess(const FaceUri& canonicalUri)
{
  m_controller.start<ndn::nfd::FaceCreateCommand>(
    ControlParameters().setUri(canonicalUri.toString()),
    bind(&Base::onHubConnectSuccess, this, _1),
    bind(&Base::onHubConnectError, this, _1));
}

void
Base::onCanonizeFailure(const std::string& reason)
{
  BOOST_THROW_EXCEPTION(Error("FaceUri canonization failed: " + reason));
}

void
Base::onHubConnectSuccess(const ControlParameters& resp)
{
  std::cerr << "Successfully created face: " << resp << std::endl;

  registerAutoConfigNames(resp.getFaceId());
}

void
Base::onHubConnectError(const ControlResponse& response)
{
  // If face exists, continue proceeding with the existing face
  if (response.getCode() == 409) {
    std::cerr << "Face exists. Proceeding with existing face: " << ControlParameters(response.getBody()) << std::endl;

    registerAutoConfigNames(ControlParameters(response.getBody()).getFaceId());
  }
  // Otherwise, report the failure and throw out exception
  else {
    std::ostringstream os;
    os << "Failed to create face: " << response.getText() << " (code: " << response.getCode() << ")";
    BOOST_THROW_EXCEPTION(Error(os.str()));
  }
}

void
Base::registerAutoConfigNames(uint64_t faceId)
{
  static const Name TESTBED_PREFIX = "/ndn";
  registerPrefix(TESTBED_PREFIX, faceId);

  static const Name LOCALHOP_NFD_PREFIX = "/localhop/nfd";
  registerPrefix(LOCALHOP_NFD_PREFIX, faceId);
}

void
Base::registerPrefix(const Name& prefix, uint64_t faceId)
{
  // Register a prefix in RIB
  m_controller.start<ndn::nfd::RibRegisterCommand>(
    ControlParameters()
      .setName(prefix)
      .setFaceId(faceId)
      .setOrigin(ndn::nfd::ROUTE_ORIGIN_AUTOCONF)
      .setCost(100)
      .setExpirationPeriod(time::milliseconds::max()),
    bind(&Base::onPrefixRegistrationSuccess, this, _1),
    bind(&Base::onPrefixRegistrationError, this, _1));
}

void
Base::onPrefixRegistrationSuccess(const ControlParameters& commandSuccessResult)
{
  std::cerr << "Successful in name registration: " << commandSuccessResult << std::endl;
}

void
Base::onPrefixRegistrationError(const ControlResponse& response)
{
  BOOST_THROW_EXCEPTION(Error("Failed in name registration, " + response.getText() +
                              " (code: " + to_string(response.getCode()) + ")"));
}


} // namespace autoconfig
} // namespace tools
} // namespace ndn