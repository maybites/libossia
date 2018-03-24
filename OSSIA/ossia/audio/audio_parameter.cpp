#include "audio_parameter.hpp"
#include "audio_protocol.hpp"
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/network/midi/midi_protocol.hpp>

namespace ossia
{

audio_parameter::audio_parameter(net::node_base& n)
  : ossia::net::parameter_base{n}
{
}

void audio_parameter::clone_value(audio_vector& res_vec) const
{
  if(res_vec.size() < audio.size())
    res_vec.resize(audio.size());

  for(std::size_t chan = 0; chan < res_vec.size(); chan++)
  {
    auto& src = audio[chan];
    auto& res = res_vec[chan];

    const std::size_t N = src.size();

    if(res.size() < N)
      res.resize(N);

    for (std::size_t i = 0; i < N; i++)
      res[i] += double(src[i]);
  }
}

void audio_parameter::push_value(const audio_port& port)
{
  auto min_chan = std::min(port.samples.size(), (std::size_t)audio.size());
  for(std::size_t chan = 0; chan < min_chan; chan++)
  {
    auto& src = port.samples[chan];
    auto& dst = audio[chan];
    const auto N = std::min(src.size(), (std::size_t)dst.size());
    for(std::size_t i = 0; i < N; i++)
    {
      dst[i] += float(src[i] * m_gain);
    }
  }
}

void audio_parameter::pull_value()
{
}

ossia::net::parameter_base& audio_parameter::push_value(const ossia::value& v)
{
  return set_value(v);
}

ossia::net::parameter_base& audio_parameter::push_value(ossia::value&& v)
{
  return set_value(v);
}

net::parameter_base&audio_parameter::push_value()
{
  return *this;
}

value audio_parameter::value() const
{
  return m_gain;
}

net::parameter_base&audio_parameter::set_value(const ossia::value& v)
{
  auto flt = ossia::convert<float>(v);
  auto vol = ossia::clamp(flt, 0.f, 1.f);
  if(m_gain != vol)
  {
    m_gain = vol;
    std::cerr << m_gain << std::endl;
    send(vol);
  }
  return *this;
}

net::parameter_base&audio_parameter::set_value(ossia::value&& v)
{
  return set_value(v);
}

val_type audio_parameter::get_value_type() const
{
  return ossia::val_type::FLOAT;
}

net::parameter_base&audio_parameter::set_value_type(val_type)
{
  return *this;
}

access_mode audio_parameter::get_access() const
{
  return {};
}

net::parameter_base&audio_parameter::set_access(access_mode)
{
  return *this;
}

const domain&audio_parameter::get_domain() const
{
  static ossia::domain d;
  return d;
}

net::parameter_base&audio_parameter::set_domain(const domain&)
{
  return *this;
}

bounding_mode audio_parameter::get_bounding() const
{
  return {};
}

net::parameter_base&audio_parameter::set_bounding(bounding_mode)
{
  return *this;
}


sound_node::sound_node()
{
  m_outlets.push_back(&audio_out);
}

void sound_node::set_sound(const std::vector<std::vector<float>>& vec)
{
  m_data.resize(vec.size());
  for(std::size_t i = 0; i < vec.size(); i++)
  {
    m_data[i].assign(vec[i].begin(), vec[i].end());
  }
}
void sound_node::set_sound(std::vector<std::vector<double>> vec)
{
  m_data = std::move(vec);
}

sound_node::~sound_node()
{

}

void do_fade(bool start_discontinuous, bool end_discontinuous, audio_channel& ap, std::size_t start, std::size_t end)
{
  using namespace std;
  if(end < start)
    swap(start, end);
  const auto decrement = (1. / (end - start));
  double gain = 1.0;

  if(!start_discontinuous && !end_discontinuous)
    return;
  else if(start_discontinuous && !end_discontinuous)
  {
    for(std::size_t j = start; j < end; j++)
    {
      ap[j] *=  (1. - gain);
      gain -= decrement;
    }
  }
  else if(!start_discontinuous && end_discontinuous)
  {
    for(std::size_t j = start; j < end; j++)
    {
      ap[j] *= std::pow(2., gain) - 1.;
      gain -= decrement;
    }
  }
  else
  {
    for(std::size_t j = start; j < end; j++)
    {
      ap[j] *= (2. * (gain * (1. - gain)));
      gain -= decrement;
    }
  }
}

void sound_node::run(ossia::token_request t, ossia::execution_state& e)
{
  if(m_data.empty())
    return;
  const std::size_t chan = m_data.size();
  const std::size_t len = m_data[0].size();

  ossia::audio_port& ap = *audio_out.data.target<ossia::audio_port>();
  ap.samples.resize(chan);
  int64_t max_N = std::min(t.date.impl, (int64_t)len);
  if(max_N <= 0)
    return;
  const auto samples = max_N - m_prev_date + t.offset.impl;
  if(samples <= 0)
    return;

  if(t.date > m_prev_date)
  {
    for(std::size_t i = 0; i < chan; i++)
    {
      ap.samples[i].resize(samples);
      for(int64_t j = m_prev_date; j < max_N; j++)
      {
        ap.samples[i][j - m_prev_date + t.offset.impl] = m_data[i][j];
      }
      do_fade(
            t.start_discontinuous,
            t.end_discontinuous,
            ap.samples[i],
            t.offset.impl,
            samples);
    }
  }
  else
  {
    // TODO rewind correctly and add rubberband
    for(std::size_t i = 0; i < chan; i++)
    {
      ap.samples[i].resize(samples);
      for(int64_t j = m_prev_date; j < max_N; j++)
      {
        ap.samples[i][max_N - (j - m_prev_date) + t.offset.impl] = m_data[i][j];
      }

      do_fade(
            t.start_discontinuous,
            t.end_discontinuous,
            ap.samples[i],
            max_N + t.offset.impl,
            m_prev_date + t.offset.impl);
    }
  }



  // Upmix
  if(upmix != 0)
  {
    if(upmix < chan)
    {
      /*
      // Downmix
      switch(upmix)
      {
        case 1:
        {
          for(std::size_t i = 1; i < chan; i++)
          {
            if(ap.samples[0].size() < ap.samples[i].size())
              ap.samples[0].resize(ap.samples[i].size());

            for(std::size_t j = 0; j < ap.samples[i].size(); j++)
              ap.samples[0][j] += ap.samples[i][j];
          }
        }
        default:
          // TODO
          break;
      }
      */
    }
    else if(upmix > chan)
    {
      switch(chan)
      {
        case 1:
        {
          ap.samples.resize(upmix);
          for(std::size_t i = 1; i < upmix; i++)
          {
            ap.samples[i] = ap.samples[0];
          }
          break;
        }
        default:
          // TODO
          break;
      }
    }
  }

  // Move channels
  if(start != 0)
  {
    ap.samples.insert(ap.samples.begin(), start, ossia::audio_channel{});
  }
}

virtual_audio_parameter::~virtual_audio_parameter()
{

}

mapped_audio_parameter::mapped_audio_parameter(bool output, audio_mapping m, net::node_base& n)
  : audio_parameter{n}
  , mapping(std::move(m))
  , is_output{output}
{
  auto& proto = static_cast<ossia::audio_protocol&>(n.get_device().get_protocol());
  proto.register_parameter(*this);
}

mapped_audio_parameter::~mapped_audio_parameter()
{
  auto& proto = static_cast<ossia::audio_protocol&>(get_node().get_device().get_protocol());
  proto.unregister_parameter(*this);

}


}
