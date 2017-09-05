// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia-max/src/parameter_base.hpp>

#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia-max/src/utils.hpp>

#include <sstream>
#include <algorithm>

namespace ossia {
namespace max {

parameter_base::parameter_base(t_class* x)
  : object_base{x}
{ }

void parameter_base::update_attribute(parameter_base* x, ossia::string_view attribute)
{
  if ( attribute == ossia::net::text_value_type() ){
    get_type(x);
  } else if ( attribute == ossia::net::text_domain() ){
    // get_domain(x);
    object_post((t_object*)x, "update domain attribute");
  } else if ( attribute == ossia::net::text_access_mode() ){
    get_access_mode(x);
  } else if ( attribute == ossia::net::text_bounding_mode() ){
    get_bounding_mode(x);
  } else if ( attribute == ossia::net::text_disabled() ){
    object_post((t_object*)x, "update enable attribute");
  } else if ( attribute == ossia::net::text_repetition_filter() ){
    object_post((t_object*)x, "update repetition_filter attribute");
  } else if ( attribute == ossia::net::text_default_value() ) {
    object_post((t_object*)x, "update default value");
  } else {
    object_base::update_attribute((node_base*)x, attribute);
  }
}

void parameter_base::set_access_mode()
{
  for (t_matcher& m : m_matchers)
  {
    ossia::net::node_base* node = m.get_node();
    auto param = node->get_parameter();

    std::string access_mode = m_access_mode->s_name;
    ossia::transform(access_mode, access_mode.begin(), ::tolower);
    m_access_mode = gensym(access_mode.c_str());

    param->set_access(symbol2access_mode(m_access_mode));
  }
}

void parameter_base::set_repetition_filter()
{
  for (t_matcher& m : m_matchers)
  {
    ossia::net::node_base* node = m.get_node();
    auto param = node->get_parameter();
    param->set_repetition_filter(
          m_repetition_filter ? ossia::repetition_filter::ON
                              : ossia::repetition_filter::OFF);
  }
}

void parameter_base::set_enable()
{
  for (t_matcher& m : m_matchers)
  {
    ossia::net::node_base* node = m.get_node();
    ossia::net::set_disabled(*node, !m_enable);
  }
}

void parameter_base::set_type()
{
  for (t_matcher& m : m_matchers)
  {
    ossia::net::node_base* node = m.get_node();
    ossia::net::parameter_base* param = node->get_parameter();
    param->set_value_type(symbol2val_type(m_type));
  }
}

void parameter_base::set_rate()
{
  for (t_matcher& m : m_matchers)
  {
    ossia::net::node_base* node = m.get_node();
    ossia::net::set_refresh_rate(*node,m_rate);
  }
}

void parameter_base::set_minmax(){
  for (t_matcher& m : m_matchers)
  {
    ossia::net::node_base* node = m.get_node();
    ossia::net::parameter_base* param = node->get_parameter();

    std::vector<ossia::value> min = attribute2value(m_min, m_min_size);
    std::vector<ossia::value> max = attribute2value(m_max, m_max_size);

    if (min.empty())
    {
      switch( param->get_value_type() )
      {
        case ossia::val_type::CHAR:
          min = {0};
          break;
        case ossia::val_type::FLOAT:
        case ossia::val_type::INT:
          min = {0.};
          break;
        case ossia::val_type::VEC2F:
          min = {0.,0.};
          break;
        case ossia::val_type::VEC3F:
          min = {0.,0.,0.};
          break;
        case ossia::val_type::VEC4F:
          min = {0.,0.,0.,0.};
          break;
        default:
          ;
      }
    }

    if ( max.empty() )
    {
      switch( param->get_value_type() )
      {
        case ossia::val_type::CHAR:
          min = {255};
          break;
        case ossia::val_type::FLOAT:
        case ossia::val_type::INT:
          min = {1.};
          break;
        case ossia::val_type::VEC2F:
          min = {1.,1.};
          break;
        case ossia::val_type::VEC3F:
          min = {1.,1.,1.};
          break;
        case ossia::val_type::VEC4F:
          min = {1.,1.,1.,1.};
          break;
        default:
          ;
      }
    }

    if (!min.empty() && !max.empty())
      param->set_domain(ossia::make_domain(min,max));
  }
}

void parameter_base::set_range()
{
  for (t_matcher& m : m_matchers)
  {
    ossia::net::node_base* node = m.get_node();
    ossia::net::parameter_base* param = node->get_parameter();

    if ( param->get_value_type() == ossia::val_type::STRING )
    {
      std::vector<std::string> senum;
      for ( int i = 0; i < m_range_size; i++)
      {
        if (m_range[i].a_type == A_SYM)
          senum.push_back(m_range[i].a_w.w_sym->s_name);
        else if (m_range[i].a_type == A_FLOAT)
        {
          std::stringstream ss;
          ss << m_range[i].a_w.w_float;
          senum.push_back(ss.str());
        }
        else
          break;
      }
      param->set_domain(make_domain(senum));
    }
    else if (m_range[0].a_type == A_FLOAT && m_range[1].a_type == A_FLOAT)
    {
      switch( param->get_value_type() )
      {
        case ossia::val_type::INT:
        case ossia::val_type::FLOAT:
        case ossia::val_type::CHAR:
          param->set_domain(
                ossia::make_domain(m_range[0].a_w.w_float,m_range[1].a_w.w_float));
          break;
        default:
          {
            std::vector<ossia::value> omin, omax;
            // TODO check param size
            std::array<float, OSSIA_MAX_MAX_ATTR_SIZE> min, max;
            min.fill(m_range[0].a_w.w_float);
            max.fill(m_range[1].a_w.w_float);
            omin.assign(min.begin(), min.end());
            omax.assign(max.begin(), max.end());
            param->set_domain(ossia::make_domain(omin,omax));
          }
      }
    }
  }
}

void parameter_base::set_bounding_mode()
{
  for (t_matcher& m : m_matchers)
  {
    ossia::net::node_base* node = m.get_node();
    ossia::net::parameter_base* param = node->get_parameter();

    std::string bounding_mode = m_bounding_mode->s_name;
    ossia::transform(bounding_mode, bounding_mode.begin(), ::tolower);
    m_bounding_mode = gensym(bounding_mode.c_str());

    if (bounding_mode == "free")
      param->set_bounding(ossia::bounding_mode::FREE);
    else if (bounding_mode == "clip")
      param->set_bounding(ossia::bounding_mode::CLIP);
    else if (bounding_mode == "wrap")
      param->set_bounding(ossia::bounding_mode::WRAP);
    else if (bounding_mode == "fold")
      param->set_bounding(ossia::bounding_mode::FOLD);
    else if (bounding_mode == "low")
      param->set_bounding(ossia::bounding_mode::LOW);
    else if (bounding_mode == "high")
      param->set_bounding(ossia::bounding_mode::HIGH);
    else
    {
      object_error((t_object*)this, "unknown bounding mode: %s", bounding_mode.c_str());
    }
  }
}

void parameter_base::set_default()
{
  for (t_matcher& m : m_matchers)
  {
    ossia::net::node_base* node = m.get_node();
    ossia::net::parameter_base* param = node->get_parameter();

    switch(param->get_value_type())
    {

      case ossia::val_type::VEC4F:
        {
          if (m_default[0].a_type == A_FLOAT && m_default[1].a_type == A_FLOAT
              && m_default[2].a_type == A_FLOAT && m_default[3].a_type == A_FLOAT)
          {
            vec4f vec = make_vec(
                  m_default[0].a_w.w_float, m_default[1].a_w.w_float,
                m_default[2].a_w.w_float, m_default[3].a_w.w_float);
            ossia::net::set_default_value(*node, vec);
          }
          break;
        }
      case ossia::val_type::VEC3F:
        {
          if (m_default[0].a_type == A_FLOAT && m_default[1].a_type == A_FLOAT
              && m_default[2].a_type == A_FLOAT )
          {
            vec3f vec = make_vec(
                  m_default[0].a_w.w_float, m_default[1].a_w.w_float,
                m_default[2].a_w.w_float);
            ossia::net::set_default_value(*node, vec);
          }
          break;
        }
      case ossia::val_type::VEC2F:
        {
          if (m_default[0].a_type == A_FLOAT && m_default[1].a_type == A_FLOAT )
          {
            vec2f vec = make_vec(
                  m_default[0].a_w.w_float, m_default[1].a_w.w_float);
            ossia::net::set_default_value(*node, vec);
          }
          break;
        }
      case ossia::val_type::FLOAT:
      case ossia::val_type::CHAR:
      case ossia::val_type::INT:
      case ossia::val_type::BOOL:
        {
          if (m_default[0].a_type == A_FLOAT )
          {
            ossia::net::set_default_value(*node, m_default[0].a_w.w_float);
          }
          break;
        }
      case ossia::val_type::STRING:
        {
          if (m_default[0].a_type == A_SYM )
          {
            ossia::net::set_default_value(*node, m_default[0].a_w.w_sym->s_name);
          }
          break;
        }
      case ossia::val_type::LIST:
        {
          auto def = attribute2value(m_default, m_default_size);

          ossia::net::set_default_value(*node, def);
          break;
        }
      default:
        ;
    }
  }
}

void parameter_base::get_range(parameter_base*x)
{
  outlet_anything(x->m_dumpout, gensym("range"), x->m_range_size, x->m_range);
}

void parameter_base::get_min(parameter_base*x)
{
  outlet_anything(x->m_dumpout, gensym("min"), x->m_min_size, x->m_min);
}

void parameter_base::get_max(parameter_base*x)
{
  outlet_anything(x->m_dumpout, gensym("max"), x->m_max_size, x->m_max);
}

void parameter_base::get_bounding_mode(parameter_base*x)
{
  // assume all matchers have the same bounding_mode
  ossia::max::t_matcher& m = x->m_matchers[0];
  ossia::net::node_base* node = m.get_node();
  ossia::net::parameter_base* param = node->get_parameter();

  x->m_bounding_mode = bounding_mode2symbol(param->get_bounding());
  t_atom a;
  A_SETSYM(&a,x->m_bounding_mode);
  outlet_anything(x->m_dumpout, gensym("bounding_mode"), 1, &a);
}

void parameter_base::get_default(parameter_base*x)
{
  // assume all matchers have the same default value
  ossia::max::t_matcher& m = x->m_matchers[0];
  ossia::net::node_base* node = m.get_node();
  ossia::net::parameter_base* param = node->get_parameter();

  auto def_val = ossia::net::get_default_value(*node);

  if ( def_val ){
    std::vector<t_atom> va;
    value2atom vm{va};
    ossia::value v = *def_val;
    v.apply(vm);

    x->m_default_size = std::min(va.size(), (long unsigned int) OSSIA_MAX_MAX_ATTR_SIZE);

    for (int i=0; i < x->m_default_size; i++ )
      x->m_default[i] = va[i];
  } else {
    x->m_default_size = 0;
  }

  outlet_anything(x->m_dumpout, gensym("default"),
                  x->m_default_size, x->m_default);
}

void parameter_base::get_type(parameter_base*x)
{

  // assume all matchers have the same type
  ossia::max::t_matcher& m = x->m_matchers[0];
  ossia::net::node_base* node = m.get_node();
  ossia::net::parameter_base* param = node->get_parameter();

  x->m_type = val_type2symbol(param->get_value_type());

  t_atom a;
  A_SETSYM(&a,x->m_type);
  outlet_anything(x->m_dumpout, gensym("type"), 1, &a);
}

void parameter_base::get_access_mode(parameter_base*x)
{
  // assume all matchers have the same bounding_mode
  ossia::max::t_matcher& m = x->m_matchers[0];
  ossia::net::node_base* node = m.get_node();
  ossia::net::parameter_base* param = node->get_parameter();

  x->m_access_mode = access_mode2symbol(param->get_access());

  t_atom a;
  A_SETSYM(&a, x->m_access_mode);
  outlet_anything(x->m_dumpout, gensym("access_mode"), 1, &a);
}

void parameter_base::get_repetition_filter(parameter_base*x)
{
  // assume all matchers have the same bounding_mode
  ossia::max::t_matcher& m = x->m_matchers[0];
  ossia::net::node_base* node = m.get_node();
  ossia::net::parameter_base* param = node->get_parameter();

  x->m_repetition_filter = param->get_repetition_filter();

  t_atom a;
  A_SETFLOAT(&a, x->m_repetition_filter);
  outlet_anything(x->m_dumpout, gensym("repetition_filter"), 1, &a);
}

void parameter_base::get_enable(parameter_base*x)
{
  // assume all matchers have the same bounding_mode
  ossia::max::t_matcher& m = x->m_matchers[0];
  ossia::net::node_base* node = m.get_node();
  ossia::net::parameter_base* param = node->get_parameter();

  x->m_enable = !param->get_disabled();

  t_atom a;
  A_SETFLOAT(&a,x->m_enable);
  outlet_anything(x->m_dumpout, gensym("enable"), 1, &a);
}

void parameter_base::push(object_base* x, t_symbol* s, int argc, t_atom* argv)
{
  ossia::net::node_base* node;

  if (!x->m_mute)
  {
    for (auto& m : x->m_matchers)
    {
      node = m.get_node();
      auto parent = m.get_parent();
      auto param = node->get_parameter();

      if (node && param)
      {
        if (argc == 1)
        {
          ossia::value v;
          // convert one element array to single element
          if (argv->a_type == A_SYM)
            v = ossia::value(std::string(atom_getsym(argv)->s_name));
          else if (argv->a_type == A_FLOAT)
            v = ossia::value(atom_getfloat(argv));

          ossia::value vv;

          if ( parent->m_ounit != ossia::none )
          {
            auto src_unit = *parent->m_ounit;
            auto dst_unit = param->get_unit();

            vv = ossia::convert(v, src_unit, dst_unit);
          } else
            vv = v;

          node->get_parameter()->push_value(vv);
        }
        else
        {
          std::vector<ossia::value> list;

          if ( s && s != gensym("list") )
            list.push_back(std::string(s->s_name));

          for (; argc > 0; argc--, argv++)
          {
            if (argv->a_type == A_SYM)
              list.push_back(std::string(atom_getsym(argv)->s_name));
            else if (argv->a_type == A_FLOAT)
              list.push_back(atom_getfloat(argv));
            else
              object_error((t_object*)x, "value type not handled");
          }
          auto src_unit = *parent->m_ounit;
          auto dst_unit = param->get_unit();

          ossia::convert(list, src_unit, dst_unit);

          node->get_parameter()->push_value(list);
        }
      }
    }
  }
}

void parameter_base::bang(object_base* x)
{
  for (auto& matcher : x->m_matchers)
  {
    matcher.enqueue_value(matcher.get_node()->get_parameter()->value());
    matcher.output_value();
  }
}

void parameter_base::output_value(object_base* x)
{
  for (auto& m : x->m_matchers)
  {
    m.output_value();
  }
  clock_delay(x->m_poll_clock, x->m_rate);
}

void parameter_base::in_float(parameter_base* x, double f)
{
  t_atom a;
  A_SETFLOAT(&a,f);
  parameter_base::push(x,nullptr,1,&a);
}

void parameter_base::in_int(parameter_base* x, long int f)
{
  t_atom a;
  A_SETLONG(&a,f);
  parameter_base::push(x,nullptr,1,&a);
}

void parameter_base::in_symbol(parameter_base* x, t_symbol* f)
{
  t_atom a;
  A_SETSYM(&a,f);
  parameter_base::push(x,nullptr,1,&a);
}

void parameter_base::declare_attributes(t_class* c)
{
  object_base :: declare_attributes(c);

  class_addmethod(c, (method) parameter_base::push, "anything", A_GIMME, 0);
  class_addmethod(c, (method) parameter_base::bang, "bang",     A_NOTHING,  0);

  class_addmethod(
      c, (method)parameter_base::in_int,
      "int", A_LONG, A_GIMME, 0);
  class_addmethod(
      c, (method)parameter_base::in_float,
      "float", A_FLOAT, 0);
  class_addmethod(
      c, (method)parameter_base::in_symbol,
      "symbol", A_SYM, 0);
  class_addmethod(
      c, (method)parameter_base::push,
      "list", A_GIMME, 0);


  CLASS_ATTR_LONG(         c, "enable",            0, parameter_base, m_enable);
  class_addmethod(c, (method) parameter_base::get_enable,            "getenable",            A_NOTHING, 0);

  CLASS_ATTR_ATOM_VARSIZE(c, "default",           0, parameter_base, m_default, m_default_size, OSSIA_MAX_MAX_ATTR_SIZE);
  class_addmethod(c, (method) parameter_base::get_default,           "getdefault",           A_NOTHING, 0);

  CLASS_ATTR_ATOM_VARSIZE(c, "range",             0, parameter_base, m_range,   m_range_size,   OSSIA_MAX_MAX_ATTR_SIZE);
  class_addmethod(c, (method) parameter_base::get_range,             "getrange",             A_NOTHING, 0);

  CLASS_ATTR_ATOM_VARSIZE(c, "min",               0, parameter_base, m_min,     m_min_size,     OSSIA_MAX_MAX_ATTR_SIZE);
  class_addmethod(c, (method) parameter_base::get_min,               "getmin",               A_NOTHING, 0);

  CLASS_ATTR_ATOM_VARSIZE(c, "max",               0, parameter_base, m_max,     m_max_size,     OSSIA_MAX_MAX_ATTR_SIZE);
  class_addmethod(c, (method) parameter_base::get_max,               "getmax",               A_NOTHING, 0);

  CLASS_ATTR_SYM(      c, "bounding_mode", 0, parameter_base, m_bounding_mode);
  class_addmethod(c, (method) parameter_base::get_bounding_mode,     "getbounding_mode",     A_NOTHING, 0);

  CLASS_ATTR_SYM(c, "type", 0, parameter_base, m_type);
  class_addmethod(c, (method) parameter_base::get_type,              "gettype",              A_NOTHING, 0);

  CLASS_ATTR_SYM(c, "access_mode", 0, parameter_base, m_access_mode);
  class_addmethod(c, (method) parameter_base::get_access_mode,       "getaccess_mode",       A_NOTHING, 0);

  CLASS_ATTR_FLOAT       (c, "repetition_filter", 0, parameter_base, m_repetition_filter);
  class_addmethod(c, (method) parameter_base::get_repetition_filter, "getrepetition_filter", A_NOTHING, 0);
}

} // namespace max
} // namespace ossia
