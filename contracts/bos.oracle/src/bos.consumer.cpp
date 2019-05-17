/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */

#include "bos.oracle/bos.oracle.hpp"
#include <eosiolib/asset.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/transaction.hpp>
#include <string>
using namespace eosio;
// namespace eosio {

using eosio::asset;
using eosio::public_key;
using std::string;

void bos_oracle::subscribe(uint64_t service_id, name contract_account,
                           name action_name, std::string publickey,
                           name account, asset amount, std::string memo) {

  // //   token::transfer_action transfer_act{ token_account, { account,
  // active_permission } };
  // //          transfer_act.send( account, consumer_account, amount, memo );

  //       INLINE_ACTION_SENDER(eosio::token, transfer)(
  //          token_account, { {account, active_permission} },
  //          { account, consumer_account, amount, memo }
  //       );
  require_auth(account);
  require_auth(contract_account);

  asset price_by_month =
      get_price_by_fee_type(service_id, data_service_fee_type::fee_month);
  check(price_by_month.amount > 0 && amount >= price_by_month,
        "amount must greater than price by month");

  transfer(account, consumer_account, amount, memo);

  // add consumer
  data_consumers consumertable(_self, _self.value);
  auto consumer_itr = consumertable.find(account.value);
  if (consumer_itr == consumertable.end()) {
    consumertable.emplace(_self, [&](auto &c) {
      c.account = account;
      // c.pubkey = publickey;
      c.status = data_consumer_status::consumer_on;
      c.create_time = time_point_sec(now());
    });
  }

  // add consumer service subscription relation
  data_service_subscriptions substable(_self, _self.value);

  auto id =
      get_hash_key(get_uuu_hash(service_id, contract_account, action_name));
  auto subs_itr = substable.find(id);
  check(subs_itr == substable.end(), "contract_account exist");

  substable.emplace(_self, [&](auto &subs) {
    subs.subscription_id = id;
    subs.service_id = service_id;
    subs.contract_account = contract_account;
    subs.action_name = action_name;
    subs.payment = amount;
    subs.consumption = asset(0, core_symbol());
    subs.subscription_time = time_point_sec(now());
  });
}

void bos_oracle::payservice(uint64_t service_id, name contract_account,
                            name action_name, name account, asset amount,
                            std::string memo) {

  require_auth(account);
  require_auth(contract_account);
  check(amount.amount > 0, "amount must be greater than zero");
  transfer(account, consumer_account, amount, memo);

  data_service_subscriptions substable(_self, _self.value);

  auto id =
      get_hash_key(get_uuu_hash(service_id, contract_account, action_name));
  auto subs_itr = substable.find(id);
  check(subs_itr != substable.end(), "contract_account does not exist");

  substable.modify(subs_itr, _self,
                   [&](auto &subs) { subs.payment += amount; });

  transaction t;
  t.actions.emplace_back(
      permission_level{_self, active_permission}, _self, "confirmpay"_n,
      std::make_tuple(service_id, contract_account, action_name, amount));
  t.delay_sec = 120; // seconds
  uint128_t deferred_id =
      (uint128_t(contract_account.value) << 64) | action_name.value;
  cancel_deferred(deferred_id);
  t.send(deferred_id, _self);
}

void bos_oracle::confirmpay(uint64_t service_id, name contract_account,
                            name action_name, asset amount) {
  check(amount.amount > 0, "amount must be greater than zero");
  data_service_subscriptions substable(_self, _self.value);

  auto id =
      get_hash_key(get_uuu_hash(service_id, contract_account, action_name));
  auto subs_itr = substable.find(id);
  check(subs_itr != substable.end(), "contract_account does not exist");
  check(subs_itr->payment > amount, "payment must be greater than amount");
  substable.modify(subs_itr, _self, [&](auto &subs) {
    subs.payment -= amount;
    subs.balance += amount;
  });
}


void bos_oracle::requestdata(uint64_t service_id, name contract_account,
                             name action_name, name requester,
                             std::string request_content) {
  require_auth(requester);

  /// check service available subsrciption status subscribe
  check(data_service_status::service_in == get_service_status(service_id) &&
            data_service_subscription_status::service_subscribe ==
                get_subscription_status(service_id, contract_account,
                                        action_name),
        "service and subscription must be available");

  fee_service(service_id, contract_account, action_name,
              data_service_fee_type::fee_times);

  data_service_requests reqtable(_self, _self.value);

  reqtable.emplace(_self, [&](auto &r) {
    r.request_id = reqtable.available_primary_key();
    r.service_id = service_id;
    r.contract_account = contract_account;
    r.action_name = action_name;
    r.requester = requester;
    r.request_time = time_point_sec(now());
    r.request_content = request_content;
  });
}

// } /// namespace eosio
