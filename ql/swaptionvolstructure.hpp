/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006 Ferdinando Ametrano
 Copyright (C) 2002, 2003 RiskMap srl
 Copyright (C) 2003, 2004, 2005, 2006 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/reference/license.html>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file swaptionvolstructure.hpp
    \brief Swaption volatility structure
*/

#ifndef quantlib_swaption_volatility_structure_hpp
#define quantlib_swaption_volatility_structure_hpp

#include <ql/termstructure.hpp>
#include <ql/Math/linearinterpolation.hpp>
#include <ql/Volatilities/smilesection.hpp>

namespace QuantLib {

    //! %Swaption-volatility structure
    /*! This class is purely abstract and defines the interface of concrete
        swaption volatility structures which will be derived from this one.
    */
    class SwaptionVolatilityStructure : public TermStructure {
      public:
        /*! \name Constructors
            See the TermStructure documentation for issues regarding
            constructors.
        */
        //@{
        //! default constructor
        /*! \warning term structures initialized by means of this
                     constructor must manage their own reference date
                     by overriding the referenceDate() method.
        */
        SwaptionVolatilityStructure(const DayCounter& dc = Actual365Fixed(),
                                    BusinessDayConvention bdc = Following);
        //! initialize with a fixed reference date
        SwaptionVolatilityStructure(const Date& referenceDate,
                                    const Calendar& calendar = Calendar(),
                                    const DayCounter& dc = Actual365Fixed(),
                                    BusinessDayConvention bdc = Following);
        //! calculate the reference date based on the global evaluation date
        SwaptionVolatilityStructure(Integer settlementDays,
                                    const Calendar&,
                                    const DayCounter& dc = Actual365Fixed(),
                                    BusinessDayConvention bdc = Following);
        //@}
        virtual ~SwaptionVolatilityStructure() {}
        //! \name Volatility and Variance
        //@{
        //! returns the volatility for a given option time and swapLength
        Volatility volatility(Time optionTime,
                              Time swapLength,
                              Rate strike,
                              bool extrapolate = false) const;
        //! returns the Black variance for a given option time and swapLength
        Real blackVariance(Time optionTime,
                           Time swapLength,
                           Rate strike,
                           bool extrapolate = false) const;
		//! returns the volatility for a given option date and swap tenor
        Volatility volatility(const Date& optionDate,
                              const Period& swapTenor,
                              Rate strike,
                              bool extrapolate = false) const;
        //! returns the Black variance for a given option date and swap tenor
        Real blackVariance(const Date& optionDate,
                           const Period& swapTenor,
                           Rate strike,
                           bool extrapolate = false) const;
		//! returns the volatility for a given option tenor and swap tenor
        Volatility volatility(const Period& optionTenor,
                              const Period& swapTenor,
                              Rate strike,
                              bool extrapolate = false) const;
		//! returns the Black variance for a given option tenor and swap tenor
        Real blackVariance(const Period& optionTenor,
                           const Period& swapTenor,
                           Rate strike,
                           bool extrapolate = false) const;
        //@}
        //! \name Limits
        //@{
        #ifndef QL_DISABLE_DEPRECATED
        /* the latest option date for which the term structure can return vols
            \deprecated use maxDate instead
        */
        virtual Date maxOptionDate() const { return maxDate(); }
        /* the latest option time for which the term structure can return vols
            \deprecated use maxTime instead
        */
        virtual Time maxOptionTime() const { return maxTime(); }
        #endif
        //! the largest length for which the term structure can return vols
        virtual Period maxSwapTenor() const = 0;
        //! the largest swapLength for which the term structure can return vols
        virtual Time maxSwapLength() const;
        //! the minimum strike for which the term structure can return vols
        virtual Rate minStrike() const = 0;
        //! the maximum strike for which the term structure can return vols
        virtual Rate maxStrike() const = 0;
        //@}
        virtual boost::shared_ptr<SmileSection> smileSection(
                                                 const Date& optionDate,
                                                 const Period& swapTenor) const {
            const std::pair<Time, Time> p = convertDates(optionDate, swapTenor);
            return smileSection(p.first, p.second);
        }
        //! implements the conversion between dates and times
        virtual std::pair<Time,Time> convertDates(const Date& optionDate,
                                                  const Period& swapTenor) const;
        //! the business day convention used for option date calculation
        virtual BusinessDayConvention businessDayConvention() const;
		//! implements the conversion between optionTenors and optionDates
		Date optionDateFromTenor(const Period& optionTenor) const;
      protected:
        //! return smile section
        virtual boost::shared_ptr<SmileSection> smileSection(
            Time optionTime, Time swapLength) const = 0;
        //! implements the actual volatility calculation in derived classes
        virtual Volatility volatilityImpl(Time optionTime,
                                          Time swapLength,
                                          Rate strike) const = 0;
        virtual Volatility volatilityImpl(const Date& optionDate,
                                          const Period& swapTenor,
                                          Rate strike) const {
            const std::pair<Time, Time> p = convertDates(optionDate, swapTenor);
            return volatilityImpl(p.first, p.second, strike);
        }
        void checkRange(Time, Time, Rate strike, bool extrapolate) const;
        void checkRange(const Date& optionDate,
                        const Period& swapTenor,
                        Rate strike, bool extrapolate) const;
      private:
        BusinessDayConvention bdc_;
    };



    // inline definitions

    inline SwaptionVolatilityStructure::SwaptionVolatilityStructure(
                                                    const DayCounter& dc,
                                                    BusinessDayConvention bdc)
    : TermStructure(dc), bdc_(bdc) {}

    inline SwaptionVolatilityStructure::SwaptionVolatilityStructure(
                                                const Date& referenceDate,
                                                const Calendar& calendar,
                                                const DayCounter& dc,
                                                BusinessDayConvention bdc)
    : TermStructure(referenceDate, calendar, dc), bdc_(bdc) {}

    inline SwaptionVolatilityStructure::SwaptionVolatilityStructure(
                                                Integer settlementDays,
                                                const Calendar& calendar,
                                                const DayCounter& dc,
                                                BusinessDayConvention bdc)
    : TermStructure(settlementDays, calendar, dc), bdc_(bdc) {}

    inline BusinessDayConvention SwaptionVolatilityStructure::businessDayConvention() const {
        return bdc_;
    }

	inline Date SwaptionVolatilityStructure::optionDateFromTenor(
                                           const Period& optionTenor) const {
			return calendar().advance(referenceDate(),
									  optionTenor,
									  businessDayConvention());
	}



    inline Volatility SwaptionVolatilityStructure::volatility(
                                                     Time optionTime,
                                                     Time swapLength,
                                                     Rate strike,
                                                     bool extrapolate) const {
        checkRange(optionTime, swapLength, strike, extrapolate);
        return volatilityImpl(optionTime, swapLength, strike);
    }


    inline Real SwaptionVolatilityStructure::blackVariance(
                                                     Time optionTime,
                                                     Time swapLength,
                                                     Rate strike,
                                                     bool extrapolate) const {
        checkRange(optionTime, swapLength, strike, extrapolate);
        Volatility vol = volatilityImpl(optionTime, swapLength, strike);
        return vol*vol*optionTime;
    }

	
	inline Volatility SwaptionVolatilityStructure::volatility(
                                                     const Date& optionDate,
                                                     const Period& swapTenor,
                                                     Rate strike,
                                                     bool extrapolate) const {
        checkRange(optionDate, swapTenor, strike, extrapolate);
        return volatilityImpl(optionDate, swapTenor, strike);
    }

    inline Real SwaptionVolatilityStructure::blackVariance(
                                                     const Date& optionDate,
                                                     const Period& swapTenor,
                                                     Rate strike,
                                                     bool extrapolate) const {
        Volatility vol =
            volatility(optionDate, swapTenor, strike, extrapolate);
        const std::pair<Time, Time> p = convertDates(optionDate, swapTenor);
        return vol*vol*p.first;
    }

	inline Volatility SwaptionVolatilityStructure::volatility(
                                                    const Period& optionTenor,
                                                    const Period& swapTenor,
                                                    Rate strike,
                                                    bool extrapolate) const {
        Date optionDate = optionDateFromTenor(optionTenor); 
        return volatility(optionDate, swapTenor, strike, extrapolate);
    }

	inline Real SwaptionVolatilityStructure::blackVariance(
                                                     const Period& optionTenor,
                                                     const Period& swapTenor,
                                                     Rate strike,
                                                     bool extrapolate) const {
        Date optionDate = optionDateFromTenor(optionTenor); 
        Volatility vol =
            volatility(optionDate, swapTenor, strike, extrapolate);
        const std::pair<Time, Time> p = convertDates(optionDate, swapTenor);
        return vol*vol*p.first;
    }


    inline Time SwaptionVolatilityStructure::maxSwapLength() const {
        return timeFromReference(referenceDate()+maxSwapTenor());
    }

    inline std::pair<Time,Time>
    SwaptionVolatilityStructure::convertDates(const Date& optionDate,
                                              const Period& swapTenor) const {
        Date end = optionDate + swapTenor;
        QL_REQUIRE(end>optionDate,
                   "negative swap tenor (" << swapTenor << ") given");
        Time optionTime = timeFromReference(optionDate);
        Time timeLength = dayCounter().yearFraction(optionDate, end);
        return std::make_pair(optionTime, timeLength);
    }

    inline void SwaptionVolatilityStructure::checkRange(
             Time optionTime, Time swapLength, Rate k, bool extrapolate) const {
        TermStructure::checkRange(optionTime, extrapolate);
        QL_REQUIRE(swapLength >= 0.0,
                   "negative swapLength (" << swapLength << ") given");
        QL_REQUIRE(extrapolate || allowsExtrapolation() ||
                   swapLength <= maxSwapLength(),
                   "swapLength (" << swapLength << ") is past max curve swapLength ("
                   << maxSwapLength() << ")");
        QL_REQUIRE(extrapolate || allowsExtrapolation() ||
                   (k >= minStrike() && k <= maxStrike()),
                   "strike (" << k << ") is outside the curve domain ["
                   << minStrike() << "," << maxStrike()<< "]");
    }

    inline void SwaptionVolatilityStructure::checkRange(
             const Date& optionDate, const Period& swapTenor,
             Rate k, bool extrapolate) const {
        TermStructure::checkRange(timeFromReference(optionDate),
                                  extrapolate);
        QL_REQUIRE(swapTenor.length() > 0,
                   "negative swap tenor (" << swapTenor << ") given");
        QL_REQUIRE(extrapolate || allowsExtrapolation() ||
                   swapTenor <= maxSwapTenor(),
                   "swap tenor (" << swapTenor << ") is past max tenor ("
                   << maxSwapTenor() << ")");
        QL_REQUIRE(extrapolate || allowsExtrapolation() ||
                   (k >= minStrike() && k <= maxStrike()),
                   "strike (" << k << ") is outside the curve domain ["
                   << minStrike() << "," << maxStrike()<< "]");
    }

}

#endif
