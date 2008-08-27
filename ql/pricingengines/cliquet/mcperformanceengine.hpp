/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008 Master IMAFA - Polytech'Nice Sophia - Université de Nice Sophia Antipolis

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#ifndef quantlib_mc_performance_engine_hpp
#define quantlib_mc_performance_engine_hpp

#include <ql/instruments/cliquetoption.hpp>
#include <ql/pricingengines/mcsimulation.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/exercise.hpp>

namespace QuantLib {

    //! Pricing engine for performance options using Monte Carlo simulation
    template<class RNG = PseudoRandom, class S = Statistics>
    class MCPerformanceEngine : public CliquetOption::engine,
                                public McSimulation<SingleVariate,RNG,S> {
      public:
        typedef
        typename McSimulation<SingleVariate,RNG,S>::path_generator_type
            path_generator_type;
        typedef typename McSimulation<SingleVariate,RNG,S>::path_pricer_type
            path_pricer_type;
        typedef typename McSimulation<SingleVariate,RNG,S>::stats_type
            stats_type;
        // constructor
        MCPerformanceEngine(
             const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
             bool brownianBridge,
             bool antitheticVariate,
             Size requiredSamples,
             Real requiredTolerance,
             Size maxSamples,
             BigNatural seed);
        void calculate() const {
            McSimulation<SingleVariate,RNG,S>::calculate(requiredTolerance_,
                                                         requiredSamples_,
                                                         maxSamples_);
            results_.value = this->mcModel_->sampleAccumulator().mean();
            if (RNG::allowsErrorEstimate)
            results_.errorEstimate =
                this->mcModel_->sampleAccumulator().errorEstimate();
        }
      protected:
        // McSimulation implementation
        TimeGrid timeGrid() const;
        boost::shared_ptr<path_generator_type> pathGenerator() const {

            TimeGrid grid = this->timeGrid();
            typename RNG::rsg_type gen =
                RNG::make_sequence_generator(grid.size()-1,seed_);
            return boost::shared_ptr<path_generator_type>(
                         new path_generator_type(process_, grid,
                                                 gen, brownianBridge_));
        }
        boost::shared_ptr<path_pricer_type> pathPricer() const;
        // data members
        boost::shared_ptr<GeneralizedBlackScholesProcess> process_;
        Size requiredSamples_, maxSamples_;
        Real requiredTolerance_;
        bool brownianBridge_;
        BigNatural seed_;
    };

    class PerformanceOptionPathPricer : public PathPricer<Path> {
      public:
        PerformanceOptionPathPricer(
                                Option::Type type, Real strike,
                                const std::vector<DiscountFactor>& discounts);
        Real operator()(const Path& path) const;
      private:
        Real strike_;
        Option::Type type_;
        std::vector<DiscountFactor> discounts_;
    };


    // template definitions

    template<class RNG, class S>
    inline
    MCPerformanceEngine<RNG,S>::MCPerformanceEngine(
             const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
             bool brownianBridge,
             bool antitheticVariate,
             Size requiredSamples,
             Real requiredTolerance,
             Size maxSamples,
             BigNatural seed)
    : McSimulation<SingleVariate,RNG,S>(antitheticVariate, false),
      process_(process), requiredSamples_(requiredSamples),
      maxSamples_(maxSamples), requiredTolerance_(requiredTolerance),
      brownianBridge_(brownianBridge), seed_(seed) {
        registerWith(process_);
    }


    template <class RNG, class S>
    inline TimeGrid MCPerformanceEngine<RNG,S>::timeGrid() const {

        std::vector<Time> fixingTimes;
        for (Size i=0; i<arguments_.resetDates.size(); i++)
            fixingTimes.push_back(process_->time(arguments_.resetDates[i]));
        fixingTimes.push_back(process_->time(arguments_.exercise->lastDate()));

        return TimeGrid(fixingTimes.begin(), fixingTimes.end());
    }


    template <class RNG, class S>
    inline
    boost::shared_ptr<typename MCPerformanceEngine<RNG,S>::path_pricer_type>
    MCPerformanceEngine<RNG,S>::pathPricer() const {

        boost::shared_ptr<PercentageStrikePayoff> payoff =
            boost::dynamic_pointer_cast<PercentageStrikePayoff>(
                this->arguments_.payoff);
        QL_REQUIRE(payoff, "non-percentage payoff given");

        boost::shared_ptr<EuropeanExercise> exercise =
            boost::dynamic_pointer_cast<EuropeanExercise>(
                this->arguments_.exercise);
        QL_REQUIRE(exercise, "wrong exercise given");

        std::vector<DiscountFactor> discounts;

        for (Size k=0;k<arguments_.resetDates.size();k++) {
            discounts.push_back(this->process_->riskFreeRate()->discount(
                                                   arguments_.resetDates[k]));
        }
        discounts.push_back(this->process_->riskFreeRate()->discount(
                                            arguments_.exercise->lastDate()));

        return boost::shared_ptr<
            typename MCPerformanceEngine<RNG,S>::path_pricer_type>(
                         new PerformanceOptionPathPricer(payoff->optionType(),
                                                         payoff->strike(),
                                                         discounts));
    }

}


#endif