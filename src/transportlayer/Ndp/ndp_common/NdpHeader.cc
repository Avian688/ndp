//
// Copyright (C) 2004 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "NdpHeader.h"

namespace inet {

namespace ndp {

std::string NdpHeader::str() const
{
    std::ostringstream stream;
    stream << getClassName() << ", ";
    static const char *flagStart = " [";
    static const char *flagSepar = " ";
    static const char *flagEnd = "]";
    stream << getSrcPort() << "->" << getDestPort();
    const char *separ = flagStart;
    if (getSynBit()) {
        stream << separ << "Syn";
        separ = flagSepar;
    }
    if (getAckBit()) {
        stream << separ << "Ack=" << getAckNo();
        separ = flagSepar;
    }
    if (getRstBit()) {
        stream << separ << "Rst";
        separ = flagSepar;
    }
    if (separ == flagSepar)
        stream << flagEnd;

    stream << " Seq=" << getDataSequenceNumber()
           << ", length = " << getChunkLength();

    //TODO show NDP Options

    return stream.str();
}


} // namespace tcp

} // namespace inet

