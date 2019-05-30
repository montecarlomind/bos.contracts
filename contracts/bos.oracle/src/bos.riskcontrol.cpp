
#include "bos.oracle/bos.oracle.hpp"
#include <eosiolib/action.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.h>
#include <eosiolib/time.hpp>
#include <eosiolib/transaction.hpp>
#include <string>

/**
 * @brief 
 * 
 * @param from 
 * @param to 
 * @param quantity 
 * @param memo 
 */
void bos_oracle::transfer(name from, name to, asset quantity, string memo) {
  check(from != to, "cannot transfer to self");
  //  require_auth( from );
  check(is_account(to), "to account does not exist");
  //  auto sym = quantity.symbol.code();
  //  stats statstable( _self, sym.raw() );
  //  const auto& st = statstable.get( sym.raw() );

  //  require_recipient( from );
  //  require_recipient( to );

  check(quantity.is_valid(), "invalid quantity");
  check(quantity.amount > 0, "must transfer positive quantity");
  // check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
  check(memo.size() <= 256, "memo has more than 256 bytes");

  //   token::transfer_action transfer_act{ token_account, { account,
  //   active_permission } };
  //          transfer_act.send( account, consumer_account, amount, memo );

  //  auto payer = has_auth( to ) ? to : from;
  action(permission_level{from, "active"_n}, token_account, "transfer"_n,
         std::make_tuple(from, to, quantity, memo))
      .send();
  // INLINE_ACTION_SENDER(eosio::token, transfer)
  // (token_account, {{from, active_permission}, {to, active_permission}},
  //  {from, to, quantity, memo});
}

/// from dapp user to dapp
void bos_oracle::deposit(name from, name to, asset quantity, string memo,
                         bool is_notify) {
                           require_auth(_self);
  transfer(from, to, quantity, memo);

  auto payer = has_auth(to) ? to : from;
  add_balance(from, quantity, payer);

  if (is_notify) {
    // notify dapp
  }
}


/// from dapp to dapp user



/**
 * @brief 
 * 
 * @param from 
 * @param to 
 * @param quantity 
 * @param memo 
 */
void bos_oracle::withdraw(name from, name to, asset quantity, string memo) {
require_auth(_self);
  sub_balance(from, quantity);

  // find service
  data_service_subscriptions svcsubstable(_self, _self.value);
  auto acc_idx = svcsubstable.get_index<"byaccount"_n>();
  auto svcsubs_itr = acc_idx.find(from.value);
  check(svcsubs_itr != acc_idx.end(), "account does not subscribe services");
  // find stake
  data_service_stakes svcstaketable(_self, _self.value);
  auto svcstake_itr = svcstaketable.find(svcsubs_itr->service_id);
  check(svcstake_itr != svcstaketable.end(), " no service stake  found");
  // check(  svcstake_itr->total_stake_amount- svcstake_itr->freeze_amount >
  // quantity, " no service stake  found" );
  //
  uint64_t time_length = 1;
  if (svcstake_itr->total_stake_amount - svcstake_itr->freeze_amount >=
      quantity) {
    svcstaketable.modify(svcstake_itr, same_payer,
                         [&](auto &ss) { ss.freeze_amount += quantity; });

    transfer(from, to, quantity, memo);
    add_freeze(svcsubs_itr->service_id, from, time_point_sec(now()),
                     time_length, quantity);
  } 
  else {
    // risk guarantee
    //    data_services datasvctable( _self, _self.value );
    //    auto svc_itr=datasvctable.find(svcsubs_itr->service_id);
    //   check( svc_itr != datasvctable.end(), " no service   found" );
    // if(svc_itr->risk_control_amount.amount==0)
    // {
    //    //risk delay
    // }

    /// delay time length

    add_delay(svcsubs_itr->service_id, from, time_point_sec(now()),
                     time_length, quantity);

    transaction t;
    t.actions.emplace_back(permission_level{_self, active_permission}, _self,
                           "transfer"_n,
                           std::make_tuple(from, to, quantity, memo));
    t.delay_sec = time_length;
    uint128_t deferred_id = (uint128_t(from.value) << 64) | to.value;
    cancel_deferred(deferred_id);
    t.send(deferred_id, from);
  }
}

/**
 * @brief 
 * 
 * @param service_id 
 * @param account 
 * @param start_time 
 * @param duration 
 * @param amount 
 * @param status 
 * @param type 
 */
void bos_oracle::add_freeze_delay(uint64_t service_id, name account,
                                  time_point_sec start_time,
                                  uint64_t duration, asset amount,
                                 uint64_t type) {
 
 transfer_freeze_delays transfertable(_self, service_id);

  transfertable.emplace(_self, [&](auto &t) {
    t.transfer_id = transfertable.available_primary_key();
    t.service_id = service_id;
    t.account = account;
    t.start_time = start_time;
    t.duration = duration;
    t.amount = amount;
    t.status = freeze_delay_status::freeze_delay_start;
    t.type = type;
  });
  
}

void bos_oracle::add_freeze(uint64_t service_id, name account,
                                  time_point_sec start_time,
                                  uint64_t duration, asset amount) {
 
  std::vector<std::tuple<name, asset>> providers =
      get_provider_list(service_id);

  check(providers.size() > 0, " no provider found");

  uint64_t average_amount = amount.amount / providers.size();
  uint64_t unfreeze_amount = 0;
  uint64_t real_freeze_amount = 0;
  uint64_t finish_freeze_provider;

  std::set<name> finish_providers;
  for (const auto &p : providers) {
    real_freeze_amount = std::get<1>(p).amount;
    if (real_freeze_amount >= average_amount) {
      real_freeze_amount = average_amount;
      finish_providers.insert(std::get<0>(p));
    } else {
      unfreeze_amount += average_amount - real_freeze_amount;
    }

    freeze_asset(service_id, std::get<0>(p),
                  asset(real_freeze_amount, core_symbol()));
  }

  // unfreeze_amount  freeze amount from provider who stake amount greater than
  // base stake amount.
  unfreeze_amount =
      freeze_providers_amount(service_id, finish_providers, asset(unfreeze_amount,core_symbol()));

  add_freeze_delay(service_id, account, time_point_sec(now()),
                     duration, amount-asset(unfreeze_amount,core_symbol()), transfer_type::transfer_freeze);

  // delay
  add_delay(service_id, account, time_point_sec(now()), duration, amount);
}

void bos_oracle::add_delay(uint64_t service_id, name account,
                                  time_point_sec start_time,
                                  uint64_t duration, asset amount) {
 

   add_freeze_delay(service_id, account, time_point_sec(now()),
                     duration, amount, transfer_type::transfer_delay);

}

uint64_t bos_oracle::freeze_providers_amount(uint64_t service_id, 
                             const std::set<name>& available_providers, 
                             asset freeze_amount) {

  std::vector<std::tuple<name, asset>> providers =
      get_provider_list(service_id);

  check(providers.size() > 0, " no provider found");

  uint64_t average_amount = freeze_amount.amount / providers.size();
  uint64_t unfreeze_amount = 0;
  uint64_t real_freeze_amount = 0;
  uint64_t finish_freeze_provider;

  std::set<name> finish_providers;
  for (const auto &p : providers) {
    name account = std::get<0>(p);
    if(available_providers.find(account)==available_providers.end())
    {
      continue;
    }

    real_freeze_amount = std::get<1>(p).amount;
    if (real_freeze_amount >= average_amount) {
      real_freeze_amount = average_amount;
      finish_providers.insert(account);
    } else {
      unfreeze_amount += average_amount - real_freeze_amount;
    }

    freeze_asset(service_id, account,asset(real_freeze_amount,core_symbol()));
  }

  if (unfreeze_amount > 0 && finish_providers.size() > 0) {
      freeze_providers_amount(service_id,finish_providers,asset(unfreeze_amount,core_symbol()));
  }

  return unfreeze_amount;
}




void bos_oracle::freeze_asset(uint64_t service_id, 
                             name account, 
                             asset amount) {


 
  data_providers providertable(_self, _self.value);
  auto provider_itr = providertable.find(account.value);
  check(provider_itr != providertable.end(), "no provider found");

  data_service_provisions provisionstable(_self, service_id);

  auto provision_itr = provisionstable.find(account.value);
  check(provision_itr != provisionstable.end(),
        "account does not subscribe the service");

  providertable.modify(provider_itr, same_payer,
                       [&](auto &p) { p.total_freeze_amount += amount; });

  provisionstable.modify(provision_itr, same_payer,
                         [&](auto &p) { p.freeze_amount += amount; });


  add_freeze_log( service_id,  account,amount) ;
}

void bos_oracle::add_freeze_log(uint64_t service_id, name account,
                                asset amount) {
  account_freeze_logs freezelogtable(_self, service_id);

  freezelogtable.emplace(_self, [&](auto &t) {
    t.log_id = freezelogtable.available_primary_key();
    t.service_id = service_id;
    t.account = account;
    t.amount = amount;
  });

  add_freeze_stat(service_id, account, amount);
}

void bos_oracle::add_freeze_stat(uint64_t service_id, name account,
                                 asset amount) {

  account_freeze_stats freezestatstable(_self, service_id);
  auto freeze_stats = freezestatstable.find(account.value);
  if (freeze_stats == freezestatstable.end()) {
    freezestatstable.emplace(_self, [&](auto &f) { f.amount = amount; });
  } else {
    freezestatstable.modify(freeze_stats, same_payer,
                            [&](auto &f) { f.amount += amount; });
  }


    service_freeze_stats svcfreezestatstable(_self, service_id);
  auto svcfreeze_stats = svcfreezestatstable.find(service_id);
  if (svcfreeze_stats == svcfreezestatstable.end()) {
    svcfreezestatstable.emplace(_self, [&](auto &s) { s.amount = amount; });
  } else {
    svcfreezestatstable.modify(svcfreeze_stats, same_payer,
                               [&](auto &s) { s.amount += amount; });
  }

}

std::tuple<asset,asset> bos_oracle::get_freeze_stat(uint64_t service_id, name account) {

 account_freeze_stats freezestatstable(_self, service_id);
  auto freeze_stats = freezestatstable.find(account.value);
  check(freeze_stats != freezestatstable.end(), "");

  service_freeze_stats svcfreezestatstable(_self, service_id);
  auto svcfreeze_stats = svcfreezestatstable.find(service_id);
  check(svcfreeze_stats != svcfreezestatstable.end(), "");

  
  return std::make_tuple(freeze_stats->amount,svcfreeze_stats->amount);

}


/**
 * @brief 
 * 
 * @param service_id 
 * @param account 
 * @param start_time 
 * @param duration 
 * @param amount 
 * @param status 
 * @return uint64_t 
 */
uint64_t bos_oracle::add_guarantee(uint64_t service_id, name account,
                                   time_point_sec start_time,
                                   uint64_t duration, asset amount,
                                   uint64_t status) {
  risk_guarantees guaranteetable(_self, _self.value);

  uint64_t risk_id = 0;
  guaranteetable.emplace(_self, [&](auto &g) {
    g.risk_id = guaranteetable.available_primary_key();
    g.account = account;
    g.amount = amount;
    g.start_time = start_time;
    g.duration = duration;
    g.status = status;
    //  g.sig = sig;
    risk_id = g.risk_id;
  });

  return risk_id;
}

/**
 * @brief 
 * 
 * @param owner 
 * @param value 
 */
void bos_oracle::sub_balance(name owner, asset value) {
  accounts dapp_acnts(_self, owner.value);

  const auto &dapp =
      dapp_acnts.get(value.symbol.code().raw(), "no balance object found");
  check(dapp.balance.amount >= value.amount, "overdrawn balance");

  dapp_acnts.modify(dapp, owner, [&](auto &a) { a.balance -= value; });
}

void bos_oracle::add_balance(name owner, asset value, name ram_payer) {
  accounts dapp_acnts(_self, owner.value);
  auto dapp = dapp_acnts.find(value.symbol.code().raw());
  if (dapp == dapp_acnts.end()) {
    dapp_acnts.emplace(ram_payer, [&](auto &a) { a.balance = value; });
  } else {
    dapp_acnts.modify(dapp, same_payer, [&](auto &a) { a.balance += value; });
  }
}